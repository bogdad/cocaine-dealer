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
#include <boost/bind.hpp>

#include "cocaine/dealer/dealer.hpp"
#include "cocaine/dealer/core/response_impl.hpp"
#include "cocaine/dealer/core/dealer_impl.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"

namespace cocaine {
namespace dealer {

response_impl_t::response_impl_t(const std::string& uuid, const message_path_t& path) :
	m_uuid(uuid),
	m_path(path),
	m_response_finished(false),
	m_message_finished(false)
{}

response_impl_t::~response_impl_t() {
	boost::mutex::scoped_lock lock(m_mutex);
	m_message_finished = true;
	m_response_finished = true;
	m_chunks.clear();
}

bool
response_impl_t::get(data_container* data, double timeout) {
	boost::mutex::scoped_lock lock(m_mutex);

	// we're all done (error or choke received)
	if (m_message_finished && m_chunks.empty()) {
		return false;
	}

	// process received chunks
	if (get_chunk(data)) {
		return true;
	}

 	// block in case there's no chunk
	if (timeout < 0.0) {
		// block until received chunk
		while (!m_response_finished) { // handle spurrious wakes
			m_cond_var.wait(lock);
		}
	}
	else {
		// block until received chunk or timed out
		boost::system_time t = boost::get_system_time();
		t += boost::posix_time::milliseconds(timeout * 1000);

		progress_timer pt;

		while (!m_response_finished && pt.elapsed().as_double() < timeout) { // handle spurrious wakes
			m_cond_var.timed_wait(lock, t);
		}
	}

	// reset state
	if (m_response_finished) {
		m_response_finished = false;
	}

	// process received chunks
	if (get_chunk(data)) {
		return true;
	}

	return false;
}

void
response_impl_t::add_chunk(const boost::shared_ptr<response_chunk_t>& chunk) {
	boost::mutex::scoped_lock lock(m_mutex);

	if (m_message_finished) {
		return;
	}

	switch (chunk->rpc_code) {
		case SERVER_RPC_MESSAGE_CHUNK:
			m_chunks.push_back(chunk);
			break;

		case SERVER_RPC_MESSAGE_CHOKE:
			m_message_finished = true;
			break;

		case SERVER_RPC_MESSAGE_ERROR:
			m_chunks.push_back(chunk);
			m_message_finished = true;
			break;

		default:
			throw internal_error("response_t received chunk with invalid RPC code: %d", chunk->rpc_code);
			break;
	}

	m_response_finished = true;

	lock.unlock();
	m_cond_var.notify_one();
}

} // namespace dealer
} // namespace cocaine
