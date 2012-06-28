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

#include <stdexcept>
#include <iostream>

#include <boost/current_function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
    
#include "cocaine/dealer/heartbeats/http_hosts_fetcher.hpp"

namespace cocaine {
namespace dealer {

http_hosts_fetcher_t::http_hosts_fetcher_t() :
	m_curl(NULL)
{
	m_curl = curl_easy_init();
}

http_hosts_fetcher_t::http_hosts_fetcher_t(const service_info_t& service_info) :
	m_curl(NULL),
	m_service_info(service_info)
{
	m_curl = curl_easy_init();
}

http_hosts_fetcher_t::~http_hosts_fetcher_t() {
	curl_easy_cleanup(m_curl);
}

int
http_hosts_fetcher_t::curl_writer(char* data, size_t size, size_t nmemb, std::string* buffer_in) {
	if (buffer_in != NULL) {
		buffer_in->append(data, size * nmemb);
		return size * nmemb;
	}

	return 0;
}

bool
http_hosts_fetcher_t::get_hosts(inetv4_endpoints_t& endpoints, service_info_t& service_info) {
	bool hosts_retreval_success = get_hosts(endpoints, m_service_info.hosts_source);
	service_info = m_service_info;
	return hosts_retreval_success;
}

bool
http_hosts_fetcher_t::get_hosts(inetv4_endpoints_t& endpoints, const std::string& source) {
	CURLcode result = CURLE_OK;
	char error_buffer[CURL_ERROR_SIZE];
	std::string buffer;
	
	if (m_curl) {
		curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, error_buffer);
		curl_easy_setopt(m_curl, CURLOPT_URL, source.c_str());
		curl_easy_setopt(m_curl, CURLOPT_HEADER, 0);
		curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, curl_writer);
		curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &buffer);

		// Attempt to retrieve the remote page
		result = curl_easy_perform(m_curl);
	}
	
	if (CURLE_OK != result) {
		return false;
	}
	
	long response_code = 0;
	result = curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &response_code);
	
	if (CURLE_OK != result || response_code != 200) {
		return false;
	}
	
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
