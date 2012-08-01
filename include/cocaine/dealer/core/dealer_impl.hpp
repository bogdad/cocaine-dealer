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

#ifndef _COCAINE_DEALER_CLIENT_IMPL_HPP_INCLUDED_
#define _COCAINE_DEALER_CLIENT_IMPL_HPP_INCLUDED_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <zmq.hpp>

#include <boost/function.hpp>
#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/xpressive/xpressive.hpp>

#include "cocaine/dealer/forwards.hpp"
#include "cocaine/dealer/core/context.hpp"
#include "cocaine/dealer/core/service.hpp"
#include "cocaine/dealer/core/dealer_object.hpp"
#include "cocaine/dealer/heartbeats/heartbeats_collector.hpp"

namespace cocaine {
namespace dealer {

class dealer_impl_t :
	private boost::noncopyable,
	public dealer_object_t
{
public:
	typedef boost::shared_ptr<response_t> response_ptr_t;
	typedef std::vector<response_ptr_t> responses_list_t;

	typedef boost::shared_ptr<service_t> service_ptr_t;
	typedef std::vector<cocaine_endpoint_t> cocaine_endpoints_list_t;

	typedef std::map<std::string, service_ptr_t> services_map_t;
	typedef std::map<std::string, cocaine_endpoints_list_t> handles_endpoints_t;

public:
	explicit dealer_impl_t(const std::string& config_path);
	virtual ~dealer_impl_t();

	response_ptr_t
	send_message(const void* data,
				 size_t size,
				 const message_path_t& path,
				 const message_policy_t& policy);

	response_ptr_t
	send_message(const void* data,
				 size_t size,
				 const message_path_t& path);

	
	responses_list_t
	send_messages(const void* data,
				  size_t size,
				  const message_path_t& path,
				  const message_policy_t& policy);

	responses_list_t
	send_messages(const void* data,
				  size_t size,
				  const message_path_t& path);

	boost::shared_ptr<message_iface>
	create_message(const void* data,
				   size_t size,
				   const message_path_t& path,
				   const message_policy_t& policy);

	message_policy_t policy_for_service(const std::string& service_alias);

private:	
	void connect();
	void disconnect();

	void service_hosts_pinged_callback(const service_info_t& service_info,
									   const handles_endpoints_t& endpoints_for_handles);

	// restoring messages from storage cache
	void load_cached_messages_for_service(service_ptr_t& service);
	void storage_iteration_callback(void* data, uint64_t size, int column);

	bool regex_match(const std::string& regex_str, const std::string& value);

	boost::shared_ptr<service_t> get_service(const std::string& service_alias);

private:
	std::map<std::string, boost::xpressive::sregex> m_regex_cache;

	size_t m_messages_cache_size;

	// dealer service name mapped to service
	services_map_t m_services;

	// heartsbeat collector
	std::auto_ptr<heartbeats_collector_t> m_heartbeats_collector;

	// temporary service ptr (2 DO: refactor this utter crap)
	service_ptr_t m_restored_service_tmp_ptr;

	// synchronization
	boost::mutex m_mutex;
	boost::mutex m_regex_mutex;

	// alive state
	bool m_is_dead;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CLIENT_IMPL_HPP_INCLUDED_
