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

#include "cocaine/dealer/response.hpp"
#include "cocaine/dealer/core/response_impl.hpp"

namespace cocaine {
namespace dealer {

response_t::response_t(const std::string& uuid, const message_path_t& path) {
	m_impl.reset(new response_impl_t(uuid, path));
}

response_t::~response_t() {
	m_impl.reset();
}

bool
response_t::get(chunk_data& data, double timeout) {
	return m_impl->get(data, timeout);
}

void
response_t::add_chunk(const chunk_data& data, const chunk_info& info) {
	m_impl->add_chunk(data, info);
}

} // namespace dealer
} // namespace cocaine
