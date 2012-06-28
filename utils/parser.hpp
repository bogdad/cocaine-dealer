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

#ifndef _COCAINE_OVERSEER_PARSER_HPP_INCLUDED_
#define _COCAINE_OVERSEER_PARSER_HPP_INCLUDED_

#include <string>

#include "json/json.h"

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include "cocaine/dealer/core/dealer_object.hpp"
#include "cocaine/dealer/cocaine_node_info/cocaine_node_info.hpp"
#include "cocaine/dealer/utils/networking.hpp"

namespace cocaine {
namespace dealer {

class cocaine_node_info_parser_t : public dealer_object_t {
public:
	cocaine_node_info_parser_t() {}
	~cocaine_node_info_parser_t() {}

	bool parse(const std::string& json_string, cocaine_node_info_t& node_info) {
		Json::Value root;
		Json::Reader reader;

		if (!reader.parse(json_string, root)) {
			return false;
		}
	
		// parse apps
		const Json::Value apps = root["apps"];
		if (!apps.isObject() || !apps.size()) {
			return false;
		}

	    Json::Value::Members app_names(apps.getMemberNames());
	    for (Json::Value::Members::iterator it = app_names.begin(); it != app_names.end(); ++it) {
	    	std::string parsed_app_name(*it);
	    	Json::Value json_app_data(apps[parsed_app_name]);

	    	cocaine_node_app_info_t app_info(parsed_app_name);
	    	if (!parse_app_info(json_app_data, app_info)) {
	    		continue;
	    	}
	    	else {
	    		node_info.apps[parsed_app_name] = app_info;
	    	}
	    }

	    // parse remaining properties
	    const Json::Value jobs_props = root["jobs"];

	    if (jobs_props.isObject()) {
	    	node_info.pending_jobs = jobs_props.get("pending", 0).asInt();
	    	node_info.processed_jobs = jobs_props.get("processed", 0).asInt();
	    }

	    node_info.route = root.get("route", "").asString();
		node_info.uptime = root.get("uptime", 0.0f).asDouble();
		node_info.ip_address = m_node_ip_address;
		node_info.port = m_node_port;

		return true;
	}

private:
	bool parse_app_info(const Json::Value& json_app_data, cocaine_node_app_info_t& app_info) {
		// parse drivers
		Json::Value tasks(json_app_data["drivers"]);
    	if (!tasks.isObject() || !tasks.size()) {
			return false;
		}

		Json::Value::Members tasks_names(tasks.getMemberNames());
    	for (Json::Value::Members::iterator it = tasks_names.begin(); it != tasks_names.end(); ++it) {
    		std::string task_name(*it);
    		Json::Value task(tasks[task_name]);

    		if (!task.isObject() || !task.size()) {
				continue;
			}

			cocaine_node_task_info_t task_info(task_name);
			if (!parse_task_info(task, task_info)) {
				continue;
			}
			else {
				app_info.tasks[task_name] = task_info;
			}
		}

		// parse remaining properties
		app_info.queue_depth = json_app_data.get("queue-depth", 0).asInt();
		std::string state = json_app_data.get("state", "").asString();

		if (state == "running") {
			app_info.status = APP_STATUS_RUNNING;
		}
		else if (state == "stopping") {
			app_info.status = APP_STATUS_STOPPING;
		}
		else if (state == "stopped") {
			app_info.status = APP_STATUS_STOPPED;
		}
		else {
			app_info.status = APP_STATUS_UNKNOWN;
		}

		const Json::Value slaves_props = json_app_data["slaves"];
	    if (slaves_props.isObject()) {
	    	app_info.slaves_busy = slaves_props.get("busy", 0).asInt();
	    	app_info.slaves_total = slaves_props.get("total", 0).asInt();
	    }

		return true;
	}

	bool parse_task_info(const Json::Value& json_app_data, cocaine_node_task_info_t& task_info) {
		std::string task_type = json_app_data.get("type", "").asString();
    	if (task_type != "native-server") {
    		return false;
    	}

    	task_info.backlog = json_app_data.get("backlog", 0).asInt();
	    task_info.endpoint = json_app_data.get("endpoint", "").asString();
	    task_info.route = json_app_data.get("route", "").asString();

		return true;
	}

	unsigned int	m_node_ip_address;
	unsigned short	m_node_port;
	std::string		m_str_node_adress;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_OVERSEER_PARSER_HPP_INCLUDED_
