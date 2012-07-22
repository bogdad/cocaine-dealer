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

#ifndef _COCAINE_DEALER_NETWORKING_HPP_INCLUDED_
#define _COCAINE_DEALER_NETWORKING_HPP_INCLUDED_

#include <string>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <cocaine/dealer/utils/error.hpp>

namespace cocaine {
namespace dealer {
namespace nutils {

static int str_to_ipv4(const std::string& str) {
    int addr;
    int res = inet_pton(AF_INET, str.c_str(), &addr);

    if (0 == res) {
		throw internal_error(std::string("bad ip address: ") + str);
    }
    else if (res < 0) {
		throw internal_error("bad address translation");
    }

    return htonl(addr);
}

static std::string ipv4_to_str(int ip) {
	char buf[128];
    int addr = ntohl(ip);
    return inet_ntop(AF_INET, &addr, buf, sizeof(buf));
}

static std::string hostname_for_ipv4(const std::string& ip) {
	in_addr_t data = inet_addr(ip.c_str());
	const hostent* host_info = gethostbyaddr(&data, 4, AF_INET);

	if (host_info) {
		return std::string(host_info->h_name);
	}

	return "";
}

static std::string hostname_for_ipv4(int ip) {
	return hostname_for_ipv4(ipv4_to_str(ip));
}

static int ipv4_from_hint(const std::string& hint) {
    addrinfo hints;

    hints.ai_family     = AF_INET; // ipv4 only for now
    hints.ai_socktype   = SOCK_STREAM;
    hints.ai_flags      = 0;
    hints.ai_protocol   = 0;
    hints.ai_canonname  = NULL;
    hints.ai_addr       = NULL;
    hints.ai_next       = NULL;

    addrinfo* result = NULL;

    int retval = getaddrinfo(hint.c_str(), NULL, &hints, &result);
    if (retval != 0) {
        freeaddrinfo(result);
        return 0;
    }
 
    for (addrinfo* rp = result; rp != NULL; rp = rp->ai_next) {
        const int buff_len = 512;
        sockaddr_in* sai = (sockaddr_in*)rp->ai_addr;
        int ip = ntohl(sai->sin_addr.s_addr);
        freeaddrinfo(result);

        return ip;
    }

    freeaddrinfo(result);
    
    return 0;
}

} // namespace nutils
} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_NETWORKING_HPP_INCLUDED_
