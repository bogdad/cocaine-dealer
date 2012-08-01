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

#ifndef _COCAINE_DEALER_RESPONSE_IMPL_HPP_INCLUDED_
#define _COCAINE_DEALER_RESPONSE_IMPL_HPP_INCLUDED_

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/thread.hpp>

#include <cocaine/dealer/forwards.hpp>
#include <cocaine/dealer/structs.hpp>
#include <cocaine/dealer/utils/data_container.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace cocaine {
namespace dealer {

class response_impl_t {
public:
	response_impl_t(const std::string& uuid,
					const message_path_t& path);

	~response_impl_t();

	// 1) timeout < 0 - block indefinitely until response_t received
	// 2) timeout == 0 - check for response_t chunk and return result immediately
	// 3) timeout > 0 - check for response_t chunk with some timeout value

	bool get(chunk_data& data, double timeout);

private:
	friend class response_t;

	void add_chunk(const chunk_data& data,
				   const chunk_info& info);

	std::vector<chunk_data>			m_chunks;
	std::string						m_uuid;
	const message_path_t			m_path;
	chunk_info						m_resp_info;

	bool m_response_finished;
	bool m_message_finished;
	bool m_caught_error;

	boost::mutex				m_mutex;
	boost::condition_variable	m_cond_var;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_RESPONSE_IMPL_HPP_INCLUDED_
