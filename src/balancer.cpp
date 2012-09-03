/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#include <msgpack.hpp>

#include <boost/thread.hpp>

#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/networking.hpp"
#include "cocaine/dealer/core/balancer.hpp"

namespace cocaine {
namespace dealer {

balancer_t::balancer_t(const std::string& identity,
					   const std::vector<cocaine_endpoint_t>& endpoints,
					   const boost::shared_ptr<context_t>& ctx,
					   bool logging_enabled) :
	dealer_object_t(ctx, logging_enabled),
	m_endpoints(endpoints),
	m_current_endpoint_index(0),
	m_socket_identity(identity)
{
	std::sort(m_endpoints.begin(), m_endpoints.end());
	recreate_socket();
}

balancer_t::~balancer_t() {
	disconnect();
}

void
balancer_t::connect(const std::vector<cocaine_endpoint_t>& endpoints) {

	if (log_flag_enabled(PLOG_DEBUG)) {
		log(PLOG_DEBUG, "connect " + m_socket_identity);
	}

	if (endpoints.empty()) {
		return;
	}

	assert(m_socket);

	std::string connection_str;
	try {
		for (size_t i = 0; i < endpoints.size(); ++i) {
			connection_str = endpoints[i].endpoint;
			m_socket->connect(connection_str.c_str());
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "balancer with identity " + m_socket_identity + " could not connect to ";
		error_msg += connection_str + " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}
}

void balancer_t::disconnect() {
	if (!m_socket) {
		return;
	}

	if (log_flag_enabled(PLOG_DEBUG)) {
		log(PLOG_DEBUG, "disconnect balancer " + m_socket_identity);
	}

	m_socket.reset();
}

void	
balancer_t::update_endpoints(const std::vector<cocaine_endpoint_t>& endpoints,
							 std::vector<cocaine_endpoint_t>& missing_endpoints)
{
	std::vector<cocaine_endpoint_t> endpoints_tmp = endpoints;
	std::sort(endpoints_tmp.begin(), endpoints_tmp.end());

	if (m_endpoints.size() == endpoints_tmp.size()) {
		if (std::equal(m_endpoints.begin(), m_endpoints.end(), endpoints_tmp.begin())) {
			return;
		}
	}

	std::vector<cocaine_endpoint_t> new_endpoints;
	get_endpoints_diff(endpoints, new_endpoints, missing_endpoints);

	if (!missing_endpoints.empty()) {

		if (log_flag_enabled(PLOG_DEBUG)) {
			log(PLOG_DEBUG, "missing endpoints on " + m_socket_identity);
		}

		recreate_socket();
		connect(endpoints);
	}
	else {
		if (!new_endpoints.empty()) {

			if (log_flag_enabled(PLOG_DEBUG)) {
				log(PLOG_DEBUG, "new endpoints on " + m_socket_identity);
			}

			connect(new_endpoints);
		}
	}

	m_endpoints = endpoints;
}

void
balancer_t::get_endpoints_diff(const std::vector<cocaine_endpoint_t>& updated_endpoints,
							   std::vector<cocaine_endpoint_t>& new_endpoints,
							   std::vector<cocaine_endpoint_t>& missing_endpoints)
{
	for (size_t i = 0; i < updated_endpoints.size(); ++i) {
		if (false == std::binary_search(m_endpoints.begin(), m_endpoints.end(), updated_endpoints[i])) {
			new_endpoints.push_back(updated_endpoints[i]);
		}
	}

	for (size_t i = 0; i < m_endpoints.size(); ++i) {
		if (false == std::binary_search(updated_endpoints.begin(), updated_endpoints.end(), m_endpoints[i])) {
			missing_endpoints.push_back(m_endpoints[i]);
		}
	}
}

void
balancer_t::recreate_socket() {
	if (log_flag_enabled(PLOG_DEBUG)) {
		log(PLOG_DEBUG, "recreate_socket " + m_socket_identity);
	}

	int timeout = balancer_t::socket_timeout;
	int64_t hwm = balancer_t::socket_hwm;
	m_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_ROUTER));
	m_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
	m_socket->setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));
	m_socket->setsockopt(ZMQ_IDENTITY, m_socket_identity.c_str(), m_socket_identity.length());
}

cocaine_endpoint_t&
balancer_t::get_next_endpoint() {
	if (m_current_endpoint_index < m_endpoints.size() - 1) {
		++m_current_endpoint_index;
	}
	else {
		m_current_endpoint_index = 0;	
	}

	return m_endpoints[m_current_endpoint_index];
}

bool
balancer_t::send(boost::shared_ptr<message_iface>& message, cocaine_endpoint_t& endpoint) {
	assert(m_socket);

	try {
		// send ident
		endpoint = get_next_endpoint();
		zmq::message_t ident_chunk(endpoint.route.size());
		memcpy((void *)ident_chunk.data(), endpoint.route.data(), endpoint.route.size());

		if (true != m_socket->send(ident_chunk, ZMQ_SNDMORE)) {
			return false;
		}

		// send header
		zmq::message_t empty_message(0);
		if (true != m_socket->send(empty_message, ZMQ_SNDMORE)) {
			return false;
		}

		// send message uuid
		const std::string& uuid = message->uuid();
		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, uuid);
		zmq::message_t uuid_chunk(sbuf.size());
		memcpy((void *)uuid_chunk.data(), sbuf.data(), sbuf.size());

		if (true != m_socket->send(uuid_chunk, ZMQ_SNDMORE)) {
			return false;
		}

		// send message policy
		policy_t server_policy = message->policy().server_policy();

		if (server_policy.deadline > 0.0) {
			// awful semantics! convert deadline [timeout value] to actual [deadline time]
			time_value server_deadline = message->enqued_timestamp();
			server_deadline += server_policy.deadline;
			server_policy.deadline = server_deadline.as_double();
		}

		sbuf.clear();
        msgpack::pack(sbuf, server_policy);
		zmq::message_t policy_chunk(sbuf.size());
		memcpy((void *)policy_chunk.data(), sbuf.data(), sbuf.size());

		if (true != m_socket->send(policy_chunk, ZMQ_SNDMORE)) {
			return false;
		}

		// send data
		size_t data_size = message->size();
		zmq::message_t data_chunk(data_size);

		if (data_size > 0) {
			message->load_data();
			memcpy((void *)data_chunk.data(), message->data(), data_size);
			message->unload_data();
		}

		if (true != m_socket->send(data_chunk)) {
			return false;
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "balancer with identity " + m_socket_identity;
		error_msg += " could not send message, details: ";
		error_msg += ex.what();
		throw internal_error(error_msg);
	}

	return true;
}

bool
balancer_t::check_for_responses(int poll_timeout) const {
	assert(m_socket);

	// poll for responce
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *m_socket;
	poll_items[0].fd = 0;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[0].revents = 0;

	int socket_response = zmq_poll(poll_items, 1, poll_timeout);

	if (socket_response <= 0) {
		return false;
	}

	// in case we received response
	if ((ZMQ_POLLIN & poll_items[0].revents) == ZMQ_POLLIN) {
		return true;
	}

    return false;
}

bool
balancer_t::is_valid_rpc_code(int rpc_code) {
	switch (rpc_code) {
		case SERVER_RPC_MESSAGE_ACK:
		case SERVER_RPC_MESSAGE_CHUNK:
		case SERVER_RPC_MESSAGE_CHOKE:
		case SERVER_RPC_MESSAGE_ERROR:
			return true;
		default:
			return false;
	}

	return false;
}

bool
balancer_t::receive(boost::shared_ptr<response_chunk_t>& response) {
	zmq::message_t		chunk;
	msgpack::object		obj;

	std::string			route;
	std::string			uuid;
	std::string			data;
	int					rpc_code;

	int 				error_code = -1;
	std::string 		error_message;

	// receive route
	if (!nutils::recv_zmq_message(*m_socket, chunk, route)) {
		return false;
	}

	// receive rpc code
	if (!nutils::recv_zmq_message(*m_socket, chunk, obj)) {
		return false;
	}
	obj.convert(&rpc_code);

	if (!is_valid_rpc_code(rpc_code)) {
		return false;
	}

	// receive uuid
	if (!nutils::recv_zmq_message(*m_socket, chunk, obj)) {
		return false;
	}
	obj.convert(&uuid);

	// init response
	response.reset(new response_chunk_t);
	response->uuid			= uuid;
	response->route			= route;
	response->rpc_code		= rpc_code;

	// receive all data
	switch (rpc_code) {
		case SERVER_RPC_MESSAGE_CHUNK: {
			// receive response data
			if (!nutils::recv_zmq_message(*m_socket, chunk, data)) {
				return false;
			}

			response->data = data_container(data.data(), data.size());
		}
		break;

		case SERVER_RPC_MESSAGE_ERROR: {
			// receive error code
			if (!nutils::recv_zmq_message(*m_socket, chunk, obj)) {
				return false;
			}
		    obj.convert(&error_code);

			// receive error message
			if (!nutils::recv_zmq_message(*m_socket, chunk, obj)) {
				return false;
			}
		    obj.convert(&error_message);
		}
		break;

		default:
		break;
	}

	// 2DO довычитать оставшиеся чанки
	int64_t		more = 1;
	size_t		more_size = sizeof(more);
	int			rc = zmq_getsockopt(*m_socket, ZMQ_RCVMORE, &more, &more_size);
    assert(rc == 0);

	while (more) {
		zmq::message_t chunk;
		if (!m_socket->recv(&chunk, ZMQ_NOBLOCK)) {
			break;
		}

		int rc = zmq_getsockopt(*m_socket, ZMQ_RCVMORE, &more, &more_size);
    	assert(rc == 0);		
	}

	// log the info we received
	std::string message = "response from: %s for msg with uuid: %s, type: ";

	switch (rpc_code) {
		case SERVER_RPC_MESSAGE_ACK: {
			if (log_flag_enabled(PLOG_DEBUG)) {
				std::string times = time_value::get_current_time().as_string();
				message += "ACK (%s)";
				log(PLOG_DEBUG, message, route.c_str(), uuid.c_str(), times.c_str());
			}
		}
		break;

		case SERVER_RPC_MESSAGE_CHUNK: {
			if (log_flag_enabled(PLOG_DEBUG)) {
				std::string times = time_value::get_current_time().as_string();
				message += "CHUNK (%s)";
				log(PLOG_DEBUG, message, route.c_str(), uuid.c_str(), times.c_str());
			}
		}
		break;

		case SERVER_RPC_MESSAGE_CHOKE: {
			if (log_flag_enabled(PLOG_DEBUG)) {
				std::string times = time_value::get_current_time().as_string();
				message += "CHOKE (%s)";
				log(PLOG_DEBUG, message, route.c_str(), uuid.c_str(), times.c_str());
			}
		}
		break;

		case SERVER_RPC_MESSAGE_ERROR: {
			if (log_flag_enabled(PLOG_ERROR)) {
				std::string times = time_value::get_current_time().as_string();
				message += "ERROR (%s), error message: %s, error code: %d";
				log(PLOG_ERROR, message, route.c_str(), uuid.c_str(), times.c_str(), error_message.c_str(), error_code);
			}
		}
		break;

		default:
			return false;
	}

	return true;
}

} // namespace dealer
} // namespace cocaine
