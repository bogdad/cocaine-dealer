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

#include "cocaine/dealer/core/context.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/storage/eblob_storage.hpp"
    
namespace cocaine {
namespace dealer {

context_t::context_t(const std::string& config_path) {
	// load configuration_t from file
	if (config_path.empty()) {
		throw internal_error("config file path is empty string at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	m_config.reset(new configuration_t(config_path));

	// create logger
	switch (m_config->logger_type()) {
		case STDOUT_LOGGER: {
				m_logger.reset(new stdout_logger_t(m_config->logger_flags()));
			}
			break;
			
		case FILE_LOGGER: {
				file_logger_t* fl = new file_logger_t(m_config->logger_flags());
				fl->init(m_config->logger_file_path());
				m_logger.reset(fl);
			}
			break;
			
		case SYSLOG_LOGGER: {
				syslog_logger_t* sl = new syslog_logger_t(m_config->logger_flags());
				sl->init(m_config->logger_syslog_identity());
				m_logger.reset(sl);
			}
			break;
			
		default:
			m_logger.reset(new empty_logger_t);
			break;
	}
	
	logger()->log(PLOG_DEBUG, "loaded config: %s", config()->config_path().c_str());
	//logger()->log(config()->as_string());
	
	// create zmq context
	m_zmq_context.reset(new zmq::context_t(1));

	// create statistics collector
	//m_stats.reset(new statistics_collector(m_config, m_zmq_context, logger()));
}

context_t::~context_t() {
	m_zmq_context.reset();
	m_storage.reset();
}

boost::shared_ptr<context_t>
context_t::shared_pointer() {
    return shared_from_this();
}

void
context_t::create_storage() {
	// create eblob_t storage
	if (config()->message_cache_type() != PERSISTENT) {
		return;
	}

	logger()->log(PLOG_DEBUG, "loading cache from eblobs...");
	std::string st_path = config()->eblob_path();
	int64_t st_blob_size = config()->eblob_blob_size();
	int st_sync = config()->eblob_sync_interval();
	int thread_pool_size = config()->eblob_thread_pool_size();
	int defrag_timeout = config()->eblob_defrag_timeout();

	// create storage
	m_storage.reset(new eblob_storage_t(st_path,
										shared_pointer(),
										true,
										st_blob_size,
										st_sync,
										thread_pool_size,
										defrag_timeout));

	// create eblob_t for each service
	const configuration_t::services_list_t& services_info_list = config()->services_list();
	configuration_t::services_list_t::const_iterator it = services_info_list.begin();
	for (; it != services_info_list.end(); ++it) {
		m_storage->open_eblob(it->second.name);
	}
}

boost::shared_ptr<configuration_t>
context_t::config() {
	return m_config;
}

boost::shared_ptr<base_logger_t>
context_t::logger() {
	return m_logger;
}

boost::shared_ptr<zmq::context_t>
context_t::zmq_context() {
	return m_zmq_context;
}

//boost::shared_ptr<statistics_collector>
//context_t::stats() {
//	return m_stats;
//}

boost::shared_ptr<eblob_storage_t>
context_t::storage() {
	return m_storage;
}

} // namespace dealer
} // namespace cocaine
