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

#include "overseer.hpp"

overseer::overseer() {
	m_zmq_context.reset(new zmq::context_t(1));
}

overseer::~overseer() {
	m_zmq_sockets.clear();
	m_zmq_context.reset();
}

void
overseer::connect(const inetv4_endpoints_t& endpoints) {
	int	timeout = 0;
	std::string uuid = "b4d62dad-c275-47d7-89db-f2b9e25c6815";

	// connect to all hosts
	for (size_t i = 0; i < endpoints.size(); ++i) {	
		// create socket
		zmq_socket_ptr_t sock(new zmq::socket_t(*m_zmq_context, ZMQ_REQ));
		sock->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));	
		sock->setsockopt(ZMQ_IDENTITY, uuid.c_str(), uuid.length());

		// create connection string
		std::string host_ip_str = nutils::ipv4_to_str(endpoints[i].host.ip);
		std::string connection_str = "tcp://" + host_ip_str + ":";
		connection_str += boost::lexical_cast<std::string>(endpoints[i].port);

		// connect
		try {
			sock->connect(connection_str.c_str());
			m_zmq_sockets[endpoints[i]] = sock;
		}
		catch (const std::exception& ex) {
			app_information app_info;
			app_info.status = app_info_status::HOST_UNREACHABLE;
			m_app_stats[endpoints[i]] = app_info;
		}
	}
}

void
overseer::send_requests(const inetv4_endpoints_t& endpoints) {
	// prepare request str
	Json::Value	msg(Json::objectValue);
	msg["version"]	= 2;
	msg["action"]	= "info";

	Json::FastWriter	writer;
	std::string			info_request = writer.write(msg);

	// send requests
	for (size_t i = 0; i < endpoints.size(); ++i) {
		// check whether endpoint is already processed
		endpoints_info_map::const_iterator it;
		it = m_app_stats.find(endpoints[i]);

		if (it != m_app_stats.end()) {
			continue;
		}

		// get endpoint socket
		endpoints_socket_map::const_iterator sit;
		sit = m_zmq_sockets.find(endpoints[i]);
		if (sit == m_zmq_sockets.end()) {
			continue;
		}

		// send info
		app_information app_info;

		try {
			zmq_socket_ptr_t sock = sit->second;

			// create metadata request message
			zmq::message_t msg(info_request.length());
			memcpy((void *)msg.data(), info_request.c_str(), info_request.length());

			if (true != sock->send(msg)) {
				app_info.status = app_info_status::HOST_UNREACHABLE;
				m_app_stats[endpoints[i]] = app_info;
			}
		}
		catch (const std::exception& ex) {
			app_info.status = app_info_status::HOST_UNREACHABLE;
			m_app_stats[endpoints[i]] = app_info;
		}
	}
}

bool
overseer::receive_responces(const inetv4_endpoints_t& endpoints,
						  	int timeout)
{
	if (m_responces.size() == endpoints.size()) {
		return true;
	}

	// figure out sockets to poll
	endpoints_socket_map sockets_to_poll;
	for (size_t i = 0; i < endpoints.size(); ++i) {
		// check whether endpoint is already processed
		endpoints_info_map::const_iterator it;
		it = m_app_stats.find(endpoints[i]);

		if (it != m_app_stats.end()) {
			continue;
		}

		// get endpoint socket
		endpoints_socket_map::const_iterator sit;
		sit = m_zmq_sockets.find(endpoints[i]);
		if (sit == m_zmq_sockets.end()) {
			continue;
		}

		sockets_to_poll.insert(std::make_pair(sit->first, sit->second));
	}

	if (sockets_to_poll.empty()) {
		return false;
	}

	// create polling structures
	std::vector<zmq_pollitem_t> poll_items;

	endpoints_socket_map::iterator it = sockets_to_poll.begin();
	while (it != sockets_to_poll.end()) {
		zmq_pollitem_t poll_item;
		poll_item.socket = *(it->second);
		poll_item.fd = 0;
		poll_item.events = ZMQ_POLLIN;
		poll_item.revents = 0;

		poll_items.push_back(poll_item);

		++it;
	}

	// poll for responce
	int res = zmq_poll(&(poll_items[0]), poll_items.size(), timeout * 1000);

	if (res <= 0) {
		return false;
	}

	it = sockets_to_poll.begin();
	for (size_t i = 0; i < poll_items.size(); ++i) {
		if ((ZMQ_POLLIN & poll_items[i].revents) != ZMQ_POLLIN) {
			++it;
			continue;
		}

		zmq::message_t reply;

		try {
			zmq::socket_t& sock = (zmq::socket_t&)poll_items[i].socket;
			if (sock.recv(&reply, ZMQ_NOBLOCK)) {
				std::string data = std::string(static_cast<char*>(reply.data()), reply.size());
				m_responces.insert(std::make_pair(it->first, data));
			}
			else {
				app_information app_info;
				app_info.status = app_info_status::HOST_UNREACHABLE;
				m_app_stats[it->first] = app_info;
			}
		}
		catch (const std::exception& ex) {
			app_information app_info;
			app_info.status = app_info_status::HOST_UNREACHABLE;
			m_app_stats[it->first] = app_info;
		}

		++it;
	}

	return false;
}

void
overseer::parce_responces(const std::string& application_name) {
	endpoints_responce_map::iterator it = m_responces.begin();

	// parse responces
	while (it != m_responces.end()) {
		cocaine_node_info_t			node_info;
		cocaine_node_info_parser_t	parser;
		app_information				app_info;

		if (parser.parse(it->second, node_info)) {
			
			app_info.uptime = node_info.uptime;
			app_info.jobs_pending = node_info.pending_jobs;
			app_info.jobs_processed = node_info.processed_jobs;

			if (node_info.app_by_name(application_name, app_info.info)) {
				switch (app_info.info.status) {
					case APP_STATUS_RUNNING:
						app_info.status = app_info_status::RUNNING;
						break;
					case APP_STATUS_STOPPING:
						app_info.status = app_info_status::STOPPING;
						break;
					case APP_STATUS_STOPPED:
						app_info.status = app_info_status::STOPPED;
						break;
					default:
						app_info.status = app_info_status::UNKNOWN;
						break;
				}
			}
			else {
				app_info.status = app_info_status::NO_APP;
			}

			m_app_stats[it->first] = app_info;
		}
		else {
			app_information app_info;
			app_info.status = app_info_status::BAD_METADATA;
			m_app_stats[it->first] = app_info;
		}

		++it;
	}
}

void
overseer::fetch_apps_info(const inetv4_endpoints_t& endpoints,
						  int timeout)
{
	connect(endpoints);
	send_requests(endpoints);

	progress_timer t;
	int tmp_timeout = timeout;
	double elapsed = 0.0;

	while (elapsed < (timeout / 1000.0)) {
		tmp_timeout = timeout - (elapsed * 1000);

		if (tmp_timeout < 0) {
			tmp_timeout = 0;
		}

		if (receive_responces(endpoints, tmp_timeout)) {
			break;
		}

		elapsed = t.elapsed().as_double();
	}
}

void
overseer::fetch_application_info(const std::string& hosts_file,
								 const std::string& application_name,
								 int timeout)
{
	std::string hosts_file_abs = absolute_path(hosts_file);

	file_hosts_fetcher_t hosts_fetcher;
	inetv4_endpoints_t endpoints;

	hosts_fetcher.get_hosts(endpoints, hosts_file_abs.c_str());
	fetch_apps_info(endpoints, timeout);
	parce_responces(application_name);
}

void
overseer::display_application_info(const std::string& application_name, bool active_only) {
	if (active_only) {
		std::cout << "limit: ON\n";
	}
	else {
		std::cout << "limit: OFF\n";
	}

	std::cout << "m_app_stats size: " << m_app_stats.size() << std::endl;

	std::cout << "app: " << application_name << ", hosts (" << m_app_stats.size() << "):\n";

	endpoints_info_map::iterator it = m_app_stats.begin();

	while ( it != m_app_stats.end()) {
		app_information ai = it->second;

		if (active_only) {
			if (ai.status != app_info_status::RUNNING) {
				++it;
				continue;
			}
		}

		std::cout << " - " << it->first.as_string() << std::endl;

		std::cout << "\tstatus:    " << app_info_status::str(ai.status) << std::endl;

		if (ai.status != app_info_status::HOST_UNREACHABLE) {
			std::cout << "\tuptime:    " << std::fixed << std::setprecision(7) << ai.uptime << std::endl;

			std::stringstream slaves;
			slaves << "workers:   busy " << ai.info.slaves_busy << ", total " << ai.info.slaves_total;
			std::cout << "\t" << slaves.str() << std::endl;

			std::cout << "\tmessages:  ";
			std::cout << "in queue " << ai.info.queue_depth << ", ";
			std::cout << "pending " << ai.jobs_pending << ", ";
			std::cout << "processed " << ai.jobs_processed << std::endl;
		}

		++it;
	}
}
