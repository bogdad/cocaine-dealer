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

#include "overseer.hpp"

int
main(int argc, char** argv) {
	try {
		options_description desc("Allowed options");
		desc.add_options()
			("help", "Produce help message")
			("hosts_file,h", value<std::string>()->default_value("tests/overseer_hosts"), "Path to hosts file")
			("application,a", value<std::string>()->default_value("rimz_app@1"), "Application to keep track of")
			("timeout,t", value<int>()->default_value(3000), "Timeout in milliseconds")
			("limit,l", value<bool>()->default_value(true), "Limit endpoints display to actively running")
		;

		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return EXIT_SUCCESS;
		}

		overseer o;
		o.fetch_application_info(vm["hosts_file"].as<std::string>(),
								 vm["application"].as<std::string>(),
								 vm["timeout"].as<int>());

		o.display_application_info(vm["application"].as<std::string>(),
								   vm["limit"].as<bool>());

		return EXIT_SUCCESS;
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
