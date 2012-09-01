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

#ifndef _COCAINE_DEALER_RESPONSE_CHUNK_HPP_INCLUDED_
#define _COCAINE_DEALER_RESPONSE_CHUNK_HPP_INCLUDED_

#include <string>

#include <cocaine/dealer/utils/data_container.hpp>

namespace cocaine {
namespace dealer {

enum e_server_response_code {
    SERVER_RPC_MESSAGE_UNKNOWN = -1,
    SERVER_RPC_MESSAGE_ACK = 1,
    SERVER_RPC_MESSAGE_CHUNK,
    SERVER_RPC_MESSAGE_ERROR,
    SERVER_RPC_MESSAGE_CHOKE
};

class response_chunk_t {
public:
	response_chunk_t() :
        rpc_code(SERVER_RPC_MESSAGE_UNKNOWN),
        error_code(-1) {};

	std::string		uuid;
	std::string		route;
	data_container	data;
	timeval			received_timestamp;

	int			rpc_code;
	int			error_code;
	std::string error_message;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_RESPONSE_CHUNK_HPP_INCLUDED_
