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

#ifndef _COCAINE_DEALER_UUID_HPP_INCLUDED_
#define _COCAINE_DEALER_UUID_HPP_INCLUDED_

#include <uuid/uuid.h>

namespace cocaine {
namespace dealer {

class wuuid_t {
public:
	wuuid_t() {}

	static const std::string generate() {
		uuid_t uuid;
		char buff[37];

		uuid_generate(uuid);
		uuid_unparse(uuid, buff);
		return buff;
	}
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_UUID_HPP_INCLUDED_
