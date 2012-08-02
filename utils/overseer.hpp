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
#include <iomanip>
#include <memory>
#include <map>

#include <zmq.hpp>    

#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>

#include "json/json.h"

#include "cocaine/dealer/heartbeats/file_hosts_fetcher.hpp"

#include "cocaine/dealer/utils/progress_timer.hpp"
#include "cocaine/dealer/utils/filesystem.hpp"

#include "parser.hpp"
#include "structs.hpp"

using namespace cocaine::dealer;
using namespace boost::program_options;

class overseer {
public:
	typedef file_hosts_fetcher_t::inetv4_endpoints_t inetv4_endpoints_t;
	typedef boost::shared_ptr<zmq::socket_t> zmq_socket_ptr_t;
	typedef std::map<inetv4_endpoint_t, app_information> endpoints_info_map;
	typedef std::map<inetv4_endpoint_t, zmq_socket_ptr_t> endpoints_socket_map;
	typedef std::map<inetv4_endpoint_t, std::string> endpoints_responce_map;

public:
	overseer();
	virtual ~overseer();

	void fetch_application_info(const std::string& hosts_file,
								const std::string& application_name,
								int timeout);

	void display_application_info(const std::string& application_name,
								  bool active_only);

private:
	void fetch_apps_info(const inetv4_endpoints_t& endpoints,
						 int timeout);

	void connect(const inetv4_endpoints_t& endpoints);
	void send_requests(const inetv4_endpoints_t& endpoints);
	bool receive_responces(const inetv4_endpoints_t& endpoints,
						   int timeout);
	void parce_responces(const std::string& application_name);

	endpoints_info_map m_app_stats;

	std::auto_ptr<zmq::context_t> m_zmq_context;
	endpoints_socket_map m_zmq_sockets;
	endpoints_responce_map m_responces;
};
