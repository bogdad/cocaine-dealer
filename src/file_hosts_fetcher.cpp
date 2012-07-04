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

#include <sys/stat.h>	
#include <unistd.h>
#include <time.h>

#include <stdexcept>
#include <fstream>
#include <iostream>

#include <boost/current_function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

#include <cocaine/dealer/utils/error.hpp>
#include "cocaine/dealer/heartbeats/file_hosts_fetcher.hpp"

namespace cocaine {
namespace dealer {

file_hosts_fetcher_t::file_hosts_fetcher_t() :
	m_file_modification_time(0)
{
}

file_hosts_fetcher_t::file_hosts_fetcher_t(const service_info_t& service_info) :
	m_service_info(service_info),
	m_file_modification_time(0)
{
}

file_hosts_fetcher_t::~file_hosts_fetcher_t() {
}

bool
file_hosts_fetcher_t::get_hosts(inetv4_endpoints_t& endpoints, service_info_t& service_info) {
	bool hosts_retreval_success = get_hosts(endpoints, m_service_info.hosts_source);
	service_info = m_service_info;
	return hosts_retreval_success;
}

bool
file_hosts_fetcher_t::get_hosts(inetv4_endpoints_t& endpoints, const std::string& source) {
	std::string buffer;

	// check file modification time
	struct stat attrib;
	if (0 != stat(source.c_str(), &attrib)) {
		throw internal_error("bad hosts path: " + source);
	}

	if (attrib.st_mode & S_IFMT != S_IFREG) {
		throw internal_error("bad hosts path: " + source + ", not a file.");
	}

	if (attrib.st_mtime <= m_file_modification_time) {
		return false;
	}

	// load file
	std::string code;
	std::ifstream file;
	file.open(source.c_str(), std::ifstream::in);

	if (!file.is_open()) {
		throw internal_error("hosts file: " + source + " failed to open.");
	}

	size_t max_size = 512;
	char buff[max_size];

	while (!file.eof()) {
		memset(buff, 0, sizeof(buff));
		file.getline(buff, max_size);
		buffer += buff;
		buffer += "\n";
	}

	file.close();
	
	// get hosts from received data
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("\n");
	tokenizer tokens(buffer, sep);

	for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
		try {
			std::string line = *tok_iter;

			boost::trim(line);
			if (line.empty() || line.at(0) == '#') {
				continue;
			}

			// look for ip/port parts
			size_t where = line.find_last_of(":");

			if (where == std::string::npos) {
				endpoints.push_back(inetv4_endpoint_t(inetv4_host_t(line), default_control_port));
			}
			else {
				std::string ip = line.substr(0, where);
				std::string port = line.substr(where + 1, (line.length() - (where + 1)));

				endpoints.push_back(inetv4_endpoint_t(ip, port));
			}
		}
		catch (...) {
		}
	}

	return true;
}

} // namespace dealer
} // namespace cocaine
