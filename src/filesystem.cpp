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

#include "cocaine/dealer/utils/filesystem.hpp"

namespace cocaine {
namespace dealer {

std::string absolute_path(const std::string& path) {
	char* absolute_path = realpath(path.c_str(), NULL);

    if (NULL != absolute_path) {
    	std::string absolute_path_str(absolute_path);
    	free(absolute_path);
    	return absolute_path_str;
    }

    return path;
}

} // namespace dealer
} // namespace cocaine
