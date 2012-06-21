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

#include <boost/current_function.hpp>

#include "cocaine/dealer/dealer.hpp"
#include "cocaine/dealer/response.hpp"
#include "cocaine/dealer/core/dealer_impl.hpp"

#include "cocaine/dealer/utils/error.hpp"

namespace cocaine {
namespace dealer {

dealer_t::dealer_t(const std::string& config_path) {
	m_impl.reset(new dealer_impl_t(config_path));
}

dealer_t::~dealer_t() {
	m_impl.reset();
}

boost::shared_ptr<response_t>
dealer_t::send_message(const void* data,
					 size_t size,
					 const message_path_t& path,
					 const message_policy_t& policy)
{
	boost::mutex::scoped_lock lock(m_mutex);

	boost::shared_ptr<message_iface> msg = m_impl->create_message(data, size, path, policy);
	boost::shared_ptr<response_t> resp(new response_t(m_impl, msg->uuid(), path));
	m_impl->send_message(msg, resp);

	return resp;
}

} // namespace dealer
} // namespace cocaine
