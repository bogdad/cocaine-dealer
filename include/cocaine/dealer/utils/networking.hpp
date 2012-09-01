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

#include <zmq.hpp>

#include <cocaine/dealer/utils/error.hpp>

namespace cocaine {
namespace dealer {

class nutils {
public:
    static int         str_to_ipv4(const std::string& str);
    static std::string ipv4_to_str(int ip);
    static std::string hostname_for_ipv4(const std::string& ip);
    static std::string hostname_for_ipv4(int ip);
    static int         ipv4_from_hint(const std::string& hint);

    static bool recv_zmq_message(zmq::socket_t& sock, zmq::message_t& msg, std::string& str, int flags = ZMQ_NOBLOCK);
    static bool recv_zmq_message(zmq::socket_t& sock, zmq::message_t& msg, msgpack::object& obj, int flags = ZMQ_NOBLOCK);

    template <typename T>
    static bool recv_zmq_message(zmq::socket_t& sock,
                                 zmq::message_t& msg,
                                 T& object,
                                 int flags = ZMQ_NOBLOCK)
    {
        if (!sock.recv(&msg, flags)) {
            return false;
        }

        memcpy(&object, msg.data(), msg.size());
        return true;
    }
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_NETWORKING_HPP_INCLUDED_
