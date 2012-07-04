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

#include "cocaine/dealer/core/cached_message.hpp"
#include "cocaine/dealer/core/request_metadata.hpp"
#include "cocaine/dealer/core/persistent_data_container.hpp"
#include "cocaine/dealer/utils/data_container.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/heartbeats/heartbeats_collector.hpp"
#include "cocaine/dealer/heartbeats/http_hosts_fetcher.hpp"
#include "cocaine/dealer/heartbeats/file_hosts_fetcher.hpp"
#include "cocaine/dealer/storage/eblob_storage.hpp"

#include "cocaine/dealer/core/dealer_impl.hpp"

namespace cocaine {
namespace dealer {

typedef cached_message_t<data_container, request_metadata_t> message_t;
typedef cached_message_t<persistent_data_container, persistent_request_metadata_t> p_message_t;

dealer_impl_t::dealer_impl_t(const std::string& config_path) :
	m_messages_cache_size(0),
	m_is_dead(false)
{
	// create dealer context
	std::string ctx_error_msg = "could not create dealer context at: " + std::string(BOOST_CURRENT_FUNCTION) + " ";

	char absolute_config_path[512];
    realpath(config_path.c_str(), absolute_config_path);

	try {
		boost::shared_ptr<cocaine::dealer::context_t> ctx(new cocaine::dealer::context_t(absolute_config_path));
		set_context(ctx);
	}
	catch (const std::exception& ex) {
		throw internal_error(ctx_error_msg + ex.what());
	}

	log(PLOG_INFO, "creating dealer.");

	// get services list
	const configuration_t::services_list_t& services_info_list = config()->services_list();

	// create services
	configuration_t::services_list_t::const_iterator it = services_info_list.begin();
	for (; it != services_info_list.end(); ++it) {
		boost::shared_ptr<service_t> service_ptr(new service_t(it->second, context()));

		log(PLOG_INFO, "STARTING SERVICE [%s]", it->second.name.c_str());

		if (config()->message_cache_type() == PERSISTENT) {
			load_cached_messages_for_service(service_ptr);
		}

		m_services[it->first] = service_ptr;
	}

	connect();
	log(PLOG_INFO, "dealer created.");
}

dealer_impl_t::~dealer_impl_t() {
	m_is_dead = true;
	disconnect();
	log(PLOG_INFO, "dealer destroyed.");
}

void
dealer_impl_t::connect() {
	log(PLOG_DEBUG, "creating heartbeats collector");
	m_heartbeats_collector.reset(new heartbeats_collector_t(context()));
	m_heartbeats_collector->set_callback(boost::bind(&dealer_impl_t::service_hosts_pinged_callback, this, _1, _2));
	m_heartbeats_collector->run();
}

void
dealer_impl_t::disconnect() {
	assert(m_heartbeats_collector.get());

	// stop collecting heartbeats
	m_heartbeats_collector.reset();

	// stop services
	services_map_t::iterator it = m_services.begin();
	for (; it != m_services.end(); ++it) {
		assert(it->second);
		it->second.reset();
	}

	m_services.clear();
}

void
dealer_impl_t::service_hosts_pinged_callback(const service_info_t& service_info,
											 const handles_endpoints_t& endpoints_for_handles)
{
	// find corresponding service
	services_map_t::iterator it = m_services.find(service_info.name);

	// populate service with pinged hosts and handles
	if (it != m_services.end()) {
		assert(it->second);
		it->second->refresh_handles(endpoints_for_handles);
	}
	else {
		std::string error_msg = "service with name " + service_info.name;
		error_msg += " was not found in services. at: " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}
}

boost::shared_ptr<message_iface>
dealer_impl_t::create_message(const void* data,
							size_t size,
							const message_path_t& path,
							const message_policy_t& policy)
{
	boost::mutex::scoped_lock lock(m_mutex);
	boost::shared_ptr<message_iface> msg;

	if (config()->message_cache_type() == RAM_ONLY) {
		msg.reset(new message_t(path, policy, data, size));
		//logger()->log(PLOG_DEBUG, "created message, size: %d bytes, uuid: %s", size, msg->uuid().c_str());
	}
	else if (config()->message_cache_type() == PERSISTENT) {
		p_message_t* msg_ptr = new p_message_t(path, policy, data, size);
		//logger()->log(PLOG_DEBUG, "created message, size: %d bytes, uuid: %s", size, msg_ptr->uuid().c_str());

		boost::shared_ptr<eblob_t> eb = context()->storage()->get_eblob(path.service_alias);
		
		// init metadata and write to storage
		msg_ptr->mdata_container().set_eblob(eb);
		msg_ptr->mdata_container().commit_data();
		msg_ptr->mdata_container().data_size = size;

		// init data and write to storage
		msg_ptr->data_container().set_eblob(eb, msg_ptr->uuid());
		msg_ptr->data_container().commit_data();

		log(PLOG_DEBUG, "commited message with uuid: " + msg_ptr->uuid() + " to persistent storage.");

		msg.reset(msg_ptr);
	}

	return msg;
}

std::string
dealer_impl_t::send_message(const boost::shared_ptr<message_iface>& msg,
						  const boost::shared_ptr<response_t>& response_t)
{
	BOOST_VERIFY(!m_is_dead);
	
	boost::mutex::scoped_lock lock(m_mutex);
	
	// find service to send message to
	std::string uuid;
	services_map_t::iterator it = m_services.find(msg->path().service_alias);

	if (it == m_services.end()) {
		std::string error_str = "no service with name " + msg->path().service_alias;
		error_str += " found at " + std::string(BOOST_CURRENT_FUNCTION);
		throw dealer_error(location_error, error_str);
	}

	assert(it->second);

	if (it->second->is_dead()) {
		throw dealer_error(request_error, "service %s is being killed", msg->path().service_alias.c_str());
	}

	uuid = msg->uuid();

	// assign callback
	//std::string message_str = "registering callback for message with uuid: " + msg->uuid();
	//logger()->log(PLOG_DEBUG, message_str);

	it->second->register_responder_callback(uuid, response_t);

	//message_str = "registered callback for message with uuid: " + msg->uuid();
	//logger()->log(PLOG_DEBUG, message_str);

	// send message to service
	it->second->send_message(msg);

	std::string message_str = "enqued msg (%d bytes) with uuid: %s to %s";

	log(PLOG_DEBUG,
		message_str,
		msg->size(),
		uuid.c_str(),
		msg->path().as_string().c_str());

	return uuid;
}

void
dealer_impl_t::unset_response_callback(const std::string& message_uuid,
								 	 const message_path_t& path)
{
	boost::mutex::scoped_lock lock(m_mutex);

	// check for services
	services_map_t::iterator it = m_services.find(path.service_alias);
	if (it == m_services.end()) {
		return;
	}

	assert(it->second);

	// assign to service
	it->second->unregister_responder_callback(message_uuid);

	//std::string message_str = "unregistered callback for message with uuid: " + message_uuid;
	//logger()->log(PLOG_DEBUG, message_str);
}

void
dealer_impl_t::load_cached_messages_for_service(boost::shared_ptr<service_t>& service) {	
	// validate input
	std::string service_name = service->info().name;

	if (!service) {
		std::string error_str = "object for service with name " + service_name;
		error_str += " is emty at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_str);
	}

	// show statistics
	boost::shared_ptr<eblob_t> blob = this->context()->storage()->get_eblob(service_name);
	std::string log_str = "SERVICE [%s] is restoring %d messages from persistent cache...";
	log(PLOG_DEBUG, log_str.c_str(), service_name.c_str(), (int)(blob->items_count() / 2));

	m_restored_service_tmp_ptr = service;

	// restore messages from
	if (blob->items_count() > 0) {
		eblob_t::iteration_callback_t callback;
		callback = boost::bind(&dealer_impl_t::storage_iteration_callback, this, _1, _2, _3);
		blob->iterate(callback, 0, 0);
	}

	m_restored_service_tmp_ptr.reset();
}

void
dealer_impl_t::storage_iteration_callback(void* data, uint64_t size, int column) {
	if (!m_restored_service_tmp_ptr) {
		throw internal_error("service object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	if (column == 0 && (!data || size == 0)) {
		throw internal_error("metadata is missing at: " + std::string(BOOST_CURRENT_FUNCTION));	
	}

	// get service eblob_t
	std::string service_name = m_restored_service_tmp_ptr->info().name;
	boost::shared_ptr<eblob_t> eb = context()->storage()->get_eblob(service_name);

	p_message_t* msg_ptr = new p_message_t();
	msg_ptr->mdata_container().load_data(data, size);
	msg_ptr->mdata_container().set_eblob(eb);
	msg_ptr->data_container().init_from_message_cache(eb, msg_ptr->mdata_container().uuid, size);

	// send message to service
	boost::shared_ptr<message_iface> msg(msg_ptr);
	m_restored_service_tmp_ptr->send_message(msg);
}

} // namespace dealer
} // namespace cocaine
