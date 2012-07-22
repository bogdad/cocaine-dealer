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

#ifndef _COCAINE_DEALER_HOSTS_FETCHER_IFACE_HPP_INCLUDED_
#define _COCAINE_DEALER_HOSTS_FETCHER_IFACE_HPP_INCLUDED_

#include <vector>

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

#include "cocaine/dealer/core/service_info.hpp"
#include "cocaine/dealer/core/inetv4_endpoint.hpp"

namespace cocaine {
namespace dealer {

class hosts_fetcher_iface {
public:
    hosts_fetcher_iface() {}
    explicit hosts_fetcher_iface(const service_info_t& service_info) :
        m_service_info(service_info) {}

	typedef std::vector<inetv4_endpoint_t> inetv4_endpoints_t;
    const static int default_control_port = 5000;
	virtual bool get_hosts(inetv4_endpoints_t& endpoints, service_info_t& service_info) = 0;
    virtual bool get_hosts(inetv4_endpoints_t& endpoints, const std::string& source) = 0;

protected:
    static void parse_hosts_data(const std::string& data, inetv4_endpoints_t& endpoints) {
        // get hosts from received data
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
        boost::char_separator<char> sep("\n");
        tokenizer tokens(data, sep);

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
                    // line can be hostname or ip v4 addr
                    int ip = nutils::ipv4_from_hint(line);

                    if (0 == ip) {
                        continue;
                    }
                    
                    endpoints.push_back(inetv4_endpoint_t(ip, default_control_port));
                }
                else {
                    std::string host_str = line.substr(0, where);
                    int ip = nutils::ipv4_from_hint(host_str);
                    std::string port = line.substr(where + 1, (line.length() - (where + 1)));

                    if (ip == 0) {
                        continue;
                    }

                    endpoints.push_back(inetv4_endpoint_t(ip, port));
                }
            }
            catch (...) {
            }
        }
    }

    service_info_t m_service_info;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_HOSTS_FETCHER_IFACE_HPP_INCLUDED_
