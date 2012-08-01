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

#ifndef _COCAINE_DEALER_STRUCTS_HPP_INCLUDED_
#define _COCAINE_DEALER_STRUCTS_HPP_INCLUDED_

#include <vector>
#include <map>
#include <stdexcept>

#include <time.h>

#include <msgpack.hpp>

#include <boost/cstdint.hpp>
#include <boost/shared_array.hpp>

#include <cocaine/dealer/types.hpp>
#include <cocaine/dealer/message_path.hpp>
#include <cocaine/dealer/message_policy.hpp>

namespace cocaine {
namespace dealer {

struct response_code {
	static const int unknown_error = 1;
	static const int message_chunk = 2;
	static const int message_choke = 3;
};

//response_data
struct chunk_data {
	chunk_data() : m_size(0) {}
	
    void assign(const void* data, size_t size) {
        m_data.reset(new char[size]);
        memcpy(m_data.get(), data, size);
        m_size = size;
    }

    void* data() const {
        return m_data.get();
    }

    size_t size() const {
        return m_size;
    }

private:
    boost::shared_array<char> m_data;
	size_t                    m_size;
};

struct chunk_info {
	chunk_info() : code(0) {}

	std::string    uuid;
	message_path_t path;
	int            code;
	std::string    error_msg;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_STRUCTS_HPP_INCLUDED_
