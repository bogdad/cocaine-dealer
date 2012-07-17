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

#include "cocaine/dealer/cocaine_node_info/cocaine_node_app_info.hpp"

using namespace cocaine::dealer;
using namespace boost::program_options;

namespace app_info_status {
	enum enum_val {
		RUNNING = 1,
		STOPPING,
		STOPPED,
		UNKNOWN,
		NO_APP,
		BAD_METADATA,
		HOST_UNREACHABLE
	};

	static std::string str(enum enum_val v) {
		switch (v) {
			case RUNNING:
				return "running";
			case STOPPING:
				return "stopping";
			case STOPPED:
				return "stopped";
			case UNKNOWN:
				return "unknown";
			case NO_APP:
				return "no app";
			case BAD_METADATA:
				return "bad metadata";
			case HOST_UNREACHABLE:
				return "unreachable";
			default:
				throw std::runtime_error("unknown value passed to app_info_status::str");
		}
	}
}

struct app_information {
	app_information() :
		uptime(0.0),
		jobs_pending(0),
		jobs_processed(0) {}

	cocaine_node_app_info_t info;
	app_info_status::enum_val status;
	double uptime;
	int jobs_pending;
	int jobs_processed;
};