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

#include <algorithm>

#include <boost/lexical_cast.hpp>

#include "cocaine/dealer/core/handle.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/uuid.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"
#include "cocaine/dealer/storage/eblob_storage.hpp"

namespace cocaine {
namespace dealer {

handle_t::handle_t(const handle_info_t& info,
				   const endpoints_list_t& endpoints,
				   const boost::shared_ptr<context_t>& ctx,
				   bool logging_enabled) :
	dealer_object_t(ctx, logging_enabled),
	m_info(info),
	m_endpoints(endpoints),
	m_is_running(false),
	m_is_connected(false),
	m_receiving_control_socket_ok(false)
{
	log(PLOG_DEBUG, "CREATED HANDLE " + description());

	// create message cache
	m_message_cache.reset(new message_cache_t(context(), true));

	// create control socket
	std::string conn_str = "inproc://service_control_" + description();
	m_zmq_control_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_PAIR));

	int timeout = 0;
	m_zmq_control_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
	m_zmq_control_socket->bind(conn_str.c_str());

	// run message dispatch thread
	m_is_running = true;
	m_thread = boost::thread(&handle_t::dispatch_messages, this);

	// connect to hosts 
	connect();
}

handle_t::~handle_t() {
	kill();
}

void
handle_t::kill() {
	if (!m_is_running) {
		return;
	}

	m_is_running = false;

	m_zmq_control_socket->close();
	m_zmq_control_socket.reset(NULL);

	m_thread.join();

	log(PLOG_DEBUG, "KILLED HANDLE " + description());
}

void
handle_t::dispatch_messages() {	
	std::string balancer_ident = m_info.as_string() + "." + wuuid_t().generate();
	balancer_t balancer(balancer_ident, m_endpoints, context());

	socket_ptr_t control_socket;
	establish_control_conection(control_socket);

	log(PLOG_DEBUG, "started message dispatch for " + description());

	m_last_response_timer.reset();
	m_deadlined_messages_timer.reset();
	m_control_messages_timer.reset();

	// process messages
	while (m_is_running) {
		// process incoming control messages every 200 msec
		int control_message = 0;
		              
		if (m_control_messages_timer.elapsed().as_double() > 0.2f) {
		      control_message = receive_control_messages(control_socket, 0);
		      m_control_messages_timer.reset();
		}

		if (control_message == CONTROL_MESSAGE_KILL) {
			// stop message dispatch, finalize everything
			m_is_running = false;
			break;
		}
		else if (control_message > 0) {
			dispatch_control_messages(control_message, balancer);
		}

		// send new message if any
		if (m_is_running && m_is_connected) {
			for (int i = 0; i < 100; ++i) { // batching
				if (m_message_cache->new_messages_count() == 0) {
					break;	
				}

				dispatch_next_available_message(balancer);
			}
		}

		// check for message responces
		bool received_response = false;

		int fast_poll_timeout = 30;		  // microsecs
		int long_poll_timeout = 300000;   // microsecs

		int response_poll_timeout = fast_poll_timeout;
		if (m_last_response_timer.elapsed().as_double() > 5.0f) {
			response_poll_timeout = long_poll_timeout;
		}

		if (m_is_connected && m_is_running) {
			received_response = balancer.check_for_responses(response_poll_timeout);

			// process received responce(s)
			while (received_response) {
				m_last_response_timer.reset();
				response_poll_timeout = fast_poll_timeout;

				dispatch_next_available_response(balancer);
				received_response = balancer.check_for_responses(response_poll_timeout);
			}
		}

		if (m_is_running) {
			if (m_deadlined_messages_timer.elapsed().as_double() > 1.0f) {
				process_deadlined_messages();
				m_deadlined_messages_timer.reset();
			}
		}
	}

	control_socket.reset();
	log(PLOG_DEBUG, "finished message dispatch for " + description());
}

void
handle_t::remove_from_persistent_storage(const boost::shared_ptr<response_chunk_t>& response) {
	if (config()->message_cache_type() != PERSISTENT) {
		return;
	}

	boost::shared_ptr<message_iface> sent_msg;

	if (false == m_message_cache->get_sent_message(response->route, response->uuid, sent_msg)) {
		return;
	}

	if (false == sent_msg->policy().persistent) {
		return;
	}

	// remove message from eblob
	boost::shared_ptr<eblob_t> eb = context()->storage()->get_eblob(sent_msg->path().service_alias);
	eb->remove_all(response->uuid);
}

void
handle_t::remove_from_persistent_storage(const std::string& uuid,
										 const message_policy_t& policy,
										 const std::string& alias)
{
	if (config()->message_cache_type() != PERSISTENT) {
		return;
	}

	if (false == policy.persistent) {
		return;
	}

	// remove message from eblob
	boost::shared_ptr<eblob_t> eb = context()->storage()->get_eblob(alias);
	eb->remove_all(uuid);
}

void
handle_t::dispatch_next_available_response(balancer_t& balancer) {
	boost::shared_ptr<response_chunk_t> response;

	if (!balancer.receive(response)) {
		return;
	}

	boost::shared_ptr<message_iface> sent_msg;

	switch (response->rpc_code) {
		case SERVER_RPC_MESSAGE_ACK:		
			if (m_message_cache->get_sent_message(response->route, response->uuid, sent_msg)) {
				sent_msg->set_ack_received(true);
			}
		break;

		case SERVER_RPC_MESSAGE_CHUNK:
			enqueue_response(response);
		break;

		case SERVER_RPC_MESSAGE_CHOKE:
			enqueue_response(response);

			remove_from_persistent_storage(response);
			m_message_cache->remove_message_from_cache(response->route, response->uuid);
		break;
		
		case SERVER_RPC_MESSAGE_ERROR: {
			// handle resource error
			if (response->error_code == resource_error) {
				if (m_message_cache->reshedule_message(response->route, response->uuid)) {
					if (log_flag_enabled(PLOG_WARNING)) {
						std::string message_str = "resheduled message with uuid: " + response->uuid;
						message_str += " from " + description() + ", reason: error received, error code: %d";
						message_str += ", error message: " + response->error_message;
						log(PLOG_WARNING, message_str, response->error_code);
					}
				}
			}
			else {
				enqueue_response(response);

				remove_from_persistent_storage(response);
				m_message_cache->remove_message_from_cache(response->route, response->uuid);

				if (log_flag_enabled(PLOG_ERROR)) {
					std::string message_str = "error received for message with uuid: " + response->uuid;	
					message_str += " from " + description() + ", error code: %d";
					message_str += ", error message: " + response->error_message;
					log(PLOG_ERROR, message_str, response->error_code);
				}
			}
		}
		break;

		default: {
			enqueue_response(response);

			remove_from_persistent_storage(response);
			m_message_cache->remove_message_from_cache(response->route, response->uuid);

			if (log_flag_enabled(PLOG_ERROR)) {
				std::string message_str = "unknown RPC code received for message with uuid: " + response->uuid;
				message_str += " from " + description() + ", code: %d";
				message_str += ", error message: " + response->error_message;
				log(PLOG_ERROR, message_str, response->error_code);
			}
		}
		break;
	}
}

namespace {
	struct resheduler {
		resheduler(const boost::shared_ptr<message_cache_t>& cache) : m_cache(cache) {
		}

		template <class T> void operator() (const T& obj) {
			m_cache->make_all_messages_new_for_route(obj.route);
		}

	private:
		boost::shared_ptr<message_cache_t> m_cache;
	};
}

void
handle_t::dispatch_control_messages(int type, balancer_t& balancer) {
	if (!m_is_running) {
		return;
	}

	switch (type) {
		case CONTROL_MESSAGE_CONNECT:
			if (!m_is_connected) {
				balancer.connect(m_endpoints);
				m_is_connected = true;
			}
			break;

		case CONTROL_MESSAGE_UPDATE:
			if (m_is_connected) {
				std::vector<cocaine_endpoint_t> missing_endpoints;
				balancer.update_endpoints(m_endpoints, missing_endpoints);

				if (!missing_endpoints.empty()) {
					std::for_each(missing_endpoints.begin(), missing_endpoints.end(), resheduler(m_message_cache));
					//m_message_cache->make_all_messages_new();
				}
			}
			break;

		case CONTROL_MESSAGE_DISCONNECT:
			balancer.disconnect();
			m_is_connected = false;
			break;
	}
}

boost::shared_ptr<message_cache_t>
handle_t::messages_cache() const {
	return m_message_cache;
}

void
handle_t::process_deadlined_messages() {
	assert(m_message_cache);
	message_cache_t::message_queue_t expired_messages;
	m_message_cache->get_expired_messages(expired_messages);

	if (expired_messages.empty()) {
		return;
	}

	std::string enqued_timestamp_str;
	std::string sent_timestamp_str;
	std::string curr_timestamp_str;

	for (size_t i = 0; i < expired_messages.size(); ++i) {
		if (log_flag_enabled(PLOG_WARNING) || log_flag_enabled(PLOG_ERROR)) {
			enqued_timestamp_str = expired_messages.at(i)->enqued_timestamp().as_string();
			sent_timestamp_str = expired_messages.at(i)->sent_timestamp().as_string();
			curr_timestamp_str = time_value::get_current_time().as_string();
		}

		if (!expired_messages.at(i)->ack_received()) {
			if (expired_messages.at(i)->can_retry()) {
				expired_messages.at(i)->increment_retries_count();
				m_message_cache->enqueue_with_priority(expired_messages.at(i));

				if (log_flag_enabled(PLOG_WARNING)) {
					std::string log_str = "no ACK, resheduled message %s, (enqued: %s, sent: %s, curr: %s)";

					log(PLOG_WARNING, log_str,
						expired_messages.at(i)->uuid().c_str(),
						enqued_timestamp_str.c_str(),
						sent_timestamp_str.c_str(),
						curr_timestamp_str.c_str());
				}
			}
			else {
				boost::shared_ptr<response_chunk_t> response(new response_chunk_t);
				response->uuid = expired_messages.at(i)->uuid();
				response->rpc_code = SERVER_RPC_MESSAGE_ERROR;
				response->error_code = request_error;
				response->error_message = "server did not reply with ack in time";
				enqueue_response(response);

				remove_from_persistent_storage(response->uuid,
											   expired_messages.at(i)->policy(),
											   expired_messages.at(i)->path().service_alias);

				if (log_flag_enabled(PLOG_WARNING)) {
					std::string log_str = "reshedule message policy exceeded, did not receive ACK ";
					log_str += "for %s, (enqued: %s, sent: %s, curr: %s)";

					log(PLOG_WARNING, log_str,
						expired_messages.at(i)->uuid().c_str(),
						enqued_timestamp_str.c_str(),
						sent_timestamp_str.c_str(),
						curr_timestamp_str.c_str());
				}
			}
		}
		else {
			boost::shared_ptr<response_chunk_t> response(new response_chunk_t);
			response->uuid = expired_messages.at(i)->uuid();
			response->rpc_code = SERVER_RPC_MESSAGE_ERROR;
			response->error_code = deadline_error;
			response->error_message = "message expired in service's handle";
			enqueue_response(response);

			remove_from_persistent_storage(response->uuid,
										   expired_messages.at(i)->policy(),
										   expired_messages.at(i)->path().service_alias);

			if (log_flag_enabled(PLOG_ERROR)) {
				std::string log_str = "deadline policy exceeded, for message %s, (enqued: %s, sent: %s, curr: %s)";

				log(PLOG_ERROR,
					log_str,
					expired_messages.at(i)->uuid().c_str(),
					enqued_timestamp_str.c_str(),
					sent_timestamp_str.c_str(),
					curr_timestamp_str.c_str());
			}
		}
	}
}

void
handle_t::establish_control_conection(socket_ptr_t& control_socket) {
	control_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_PAIR));

	if (control_socket.get()) {
		std::string conn_str = "inproc://service_control_" + description();

		int timeout = 0;
		control_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
		control_socket->connect(conn_str.c_str());
		m_receiving_control_socket_ok = true;
	}
}

void
handle_t::enqueue_response(boost::shared_ptr<response_chunk_t>& response) {
	if (m_response_callback && m_is_running) {
		m_response_callback(response);
	}
}

int
handle_t::receive_control_messages(socket_ptr_t& control_socket, int poll_timeout) {
	if (!m_is_running) {
		return 0;
	}

	// poll for responce
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *control_socket;
	poll_items[0].fd = 0;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[0].revents = 0;

	int socket_response = zmq_poll(poll_items, 1, poll_timeout);

	if (socket_response <= 0) {
		return 0;
	}

	// in case we received control message
    if ((ZMQ_POLLIN & poll_items[0].revents) != ZMQ_POLLIN) {
    	return 0;
    }

	int received_message = 0;

	bool recv_failed = false;
	zmq::message_t reply;

	try {
		if (!control_socket->recv(&reply)) {
			recv_failed = true;
		}
		else {
			memcpy((void *)&received_message, reply.data(), reply.size());
			return received_message;
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "some very ugly shit happend while recv on control socket at ";
		error_msg += std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " details: " + std::string(ex.what());
		throw internal_error(error_msg);
	}

    if (recv_failed) {
    	log(PLOG_ERROR, "control socket recv failed on " + description());
    }

    return 0;
}

bool
handle_t::dispatch_next_available_message(balancer_t& balancer) {
	// send new message if any
	if (m_message_cache->new_messages_count() == 0) {
		return false;
	}

	boost::shared_ptr<message_iface> new_msg = m_message_cache->get_new_message();
	cocaine_endpoint_t endpoint;
	if (balancer.send(new_msg, endpoint)) {
		new_msg->mark_as_sent(true);
		m_message_cache->move_new_message_to_sent(endpoint.route);

		if (log_flag_enabled(PLOG_DEBUG)) {
			std::string log_msg = "sent msg with uuid: %s to endpoint: %s with route: %s (%s)";
			std::string sent_timestamp_str = new_msg->sent_timestamp().as_string();

			log(PLOG_DEBUG,
				log_msg.c_str(),
				new_msg->uuid().c_str(),
				endpoint.endpoint.c_str(),
				description().c_str(),
				sent_timestamp_str.c_str());
		}

		return true;
	}
	else {
		log(PLOG_ERROR, "dispatch_next_available_message failed");		
	}

	return false;
}

const handle_info_t&
handle_t::info() const {
	return m_info;
}

std::string
handle_t::description() {
	return m_info.as_string();
}

void
handle_t::connect() {
	boost::mutex::scoped_lock lock(m_mutex);

	if (!m_is_running || m_endpoints.empty() || m_is_connected) {
		return;
	}

	log(PLOG_DEBUG, "CONNECT HANDLE " + description());

	// connect to hosts
	int control_message = CONTROL_MESSAGE_CONNECT;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	m_zmq_control_socket->send(message);
}

void
handle_t::update_endpoints(const std::vector<cocaine_endpoint_t>& endpoints) {
	if (!m_is_running || endpoints.empty()) {
		return;
	}

	boost::mutex::scoped_lock lock(m_mutex);
	m_endpoints = endpoints;
	lock.unlock();

	log(PLOG_DEBUG, "UPDATE HANDLE " + description());

	// connect to hosts
	int control_message = CONTROL_MESSAGE_UPDATE;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	m_zmq_control_socket->send(message);
}

void
handle_t::make_all_messages_new() {
	assert (m_message_cache);
	m_message_cache->make_all_messages_new();
}

void
handle_t::assign_message_queue(const message_cache_t::message_queue_ptr_t& message_queue) {
	assert (m_message_cache);
	m_message_cache->append_message_queue(message_queue);
}

void
handle_t::set_responce_callback(responce_callback_t callback) {
	boost::mutex::scoped_lock lock(m_mutex);
	m_response_callback = callback;
}

void
handle_t::enqueue_message(const boost::shared_ptr<message_iface>& message) {
	m_message_cache->enqueue(message);
}

} // namespace dealer
} // namespace cocaine
