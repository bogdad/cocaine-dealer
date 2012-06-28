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

#include <iostream>
#include <memory>
#include <map>

#include <zmq.hpp>    
#include <boost/program_options.hpp>

#include "json/json.h"
#include "cocaine/dealer/heartbeats/file_hosts_fetcher.hpp"
#include "parser.hpp"

using namespace cocaine::dealer;
using namespace boost::program_options;

struct app_information {
	app_information() :
		uptime(0.0),
		jobs_pending(0),
		jobs_processed(0) {}

	cocaine_node_app_info_t info;
	std::string status;
	double uptime;
	int jobs_pending;
	int jobs_processed;
};

bool fetch_app_info(const inetv4_endpoint_t& endpoint, std::string& info);
void fetch_cloud_application_info(const std::string& hosts_file,
								  const std::string& application_name);



void fetch_cloud_application_info(const std::string& hosts_file,
								  const std::string& application_name)
{
	// prepare hosts path
	char absolute_hosts_file[512];
    realpath(hosts_file.c_str(), absolute_hosts_file);

	file_hosts_fetcher_t hosts_fetcher;
	file_hosts_fetcher_t::inetv4_endpoints_t endpoints;

	// get endpoints list
	hosts_fetcher.get_hosts(endpoints, absolute_hosts_file);

	typedef std::map<inetv4_endpoint_t, app_information> endpoints_info_map;

	endpoints_info_map app_stats;

	// get endpoints data
	for (size_t i = 0; i < endpoints.size(); ++i) {
		std::string str_info;

		app_information app_info;

		if (fetch_app_info(endpoints[i], str_info)) {
			cocaine_node_info_t node_info;
			cocaine_node_info_parser_t parser;

			if (parser.parse(str_info, node_info)) {
				app_info.uptime = node_info.uptime;
				app_info.jobs_pending = node_info.pending_jobs;
				app_info.jobs_processed = node_info.processed_jobs;

				if (node_info.app_by_name(application_name, app_info.info)) {
					switch (app_info.info.status) {
						case APP_STATUS_RUNNING:
							app_info.status = "running";
							break;
						case APP_STATUS_STOPPING:
							app_info.status = "stopping";
							break;
						case APP_STATUS_STOPPED:
							app_info.status = "stopped";
							break;
						default:
							app_info.status = "unknown";
							break;
					}
				}
				else {
					app_info.status = "no app deployed";
				}
			}
			else {
				app_info.status = "bad metadata";
			}
		}
		else {
			app_info.status = "host unreachable";
		}

		app_stats[endpoints[i]] = app_info;
	}

	std::cout << "app: " << application_name << ", hosts (" << app_stats.size() << "):\n";

	for (endpoints_info_map::iterator it = app_stats.begin(); it != app_stats.end(); ++it) {
		std::cout << " - " << it->first.as_string() << std::endl;

		app_information ai = it->second;
		std::cout << "\tstatus:    " << ai.status << std::endl;

		if (ai.status != "host unreachable") {
			std::cout << "\tuptime:    " << std::fixed << std::setprecision(7) << ai.uptime << std::endl;

			std::stringstream slaves;
			slaves << "workers:   busy " << ai.info.slaves_busy << ", total " << ai.info.slaves_total;
			std::cout << "\t" << slaves.str() << std::endl;

			std::cout << "\tmessages:  ";
			std::cout << "in queue " << ai.info.queue_depth << ", ";
			std::cout << "pending " << ai.jobs_pending << ", ";
			std::cout << "processed " << ai.jobs_processed << std::endl;
		}
	}
}

bool fetch_app_info(const inetv4_endpoint_t& endpoint, std::string& info) {
	std::auto_ptr<zmq::context_t> zmq_context(new zmq::context_t(1));

	// create req socket
	std::auto_ptr<zmq::socket_t> zmq_socket;
	zmq_socket.reset(new zmq::socket_t(*zmq_context, ZMQ_REQ));
	std::string ex_err;

	// connect to host
	std::string host_ip_str = nutils::ipv4_to_str(endpoint.host.ip);
	std::string connection_str = "tcp://" + host_ip_str + ":";
	connection_str += boost::lexical_cast<std::string>(endpoint.port);

	int timeout = 0;
	zmq_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));

	std::string uuid = "b4d62dad-c275-47d7-89db-f2b9e25c6815";
	zmq_socket->setsockopt(ZMQ_IDENTITY, uuid.c_str(), uuid.length());

	try {
		zmq_socket->connect(connection_str.c_str());
	}
	catch (const std::exception& ex) {
		zmq_socket.reset();
		zmq_context.reset();

		std::cout << ex.what() << std::endl;
		return false;
	}

	// send request for cocaine metadata
	Json::Value msg(Json::objectValue);
	Json::FastWriter writer;

	msg["version"] = 2;
	msg["action"] = "info";

	std::string info_request = writer.write(msg);
	zmq::message_t message(info_request.length());
	memcpy((void *)message.data(), info_request.c_str(), info_request.length());

	bool sent_request_ok = true;

	try {
		sent_request_ok = zmq_socket->send(message);
	}
	catch (const std::exception& ex) {
		sent_request_ok = false;
		ex_err = ex.what();
	}

	if (!sent_request_ok) {
		zmq_socket.reset();
		zmq_context.reset();

		return false;
	}

	// create polling structure
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *zmq_socket;
	poll_items[0].fd = 0;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[0].revents = 0;

	// poll for responce
	int res = zmq_poll(poll_items, 1, 5000);

	if (res <= 0) {
		zmq_socket.reset();
		zmq_context.reset();

		return false;
	}

	if ((ZMQ_POLLIN & poll_items[0].revents) != ZMQ_POLLIN) {
		zmq_socket.reset();
		zmq_context.reset();

		return false;
	}

	// receive cocaine control data
	zmq::message_t reply;
	bool received_response_ok = true;

	try {
		received_response_ok = zmq_socket->recv(&reply);
		info = std::string(static_cast<char*>(reply.data()), reply.size());
	}
	catch (const std::exception& ex) {
		received_response_ok = false;
		ex_err = ex.what();
	}

	if (!received_response_ok) {
		zmq_socket.reset();
		zmq_context.reset();

		return false;
	}

	zmq_socket.reset();
	zmq_context.reset();

	return true;
}

int
main(int argc, char** argv) {
	try {
		options_description desc("Allowed options");
		desc.add_options()
			("help", "Produce help message")
			("hosts_file,h", value<std::string>()->default_value("tests/overseer_hosts"), "Path to hosts file")
			("application,t", value<std::string>()->default_value("rimz_app@1"), "Application to keep track of")
		;

		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return EXIT_SUCCESS;
		}

		fetch_cloud_application_info(vm["hosts_file"].as<std::string>(),
									 vm["application"].as<std::string>());

		return EXIT_SUCCESS;
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
