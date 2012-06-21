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

#include <cstring>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <uuid/uuid.h>

#include <openssl/sha.h>
#include <openssl/evp.h>

#include "json/json.h"

#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/data_container.hpp"

namespace cocaine {
namespace dealer {

data_container::data_container() :
	data_(NULL),
	size_(0),
	signed_(false)
{
	init();
}

data_container::data_container(const void* data, size_t size) :
	size_(size),
	signed_(false)
{
	set_data(data, size);
}

data_container::data_container(const data_container& dc) {
	if (dc.empty()) {
		init();
		return;
	}

	*this = dc;
}

void
data_container::set_data(const void* data, size_t size) {
	// clean-up in case we have anything
	clear();

	// early exit
	if (data == NULL || size == 0) {
		data_ = NULL;
		size_ = 0;
		return;
	}

	// copy provided data
	std::string error_msg = "not enough memory to create new data container at ";
	error_msg += std::string(BOOST_CURRENT_FUNCTION);

	try {
		data_ = new unsigned char[size];
	}
	catch (...) {
		throw internal_error(error_msg);
	}

	if (!data_) {
		throw internal_error(error_msg);
	}

	memcpy(data_, data, size);
	++*ref_counter_;
	size_ = size;

	// data too small to compare by sha, we'll do comparisons with memcmp
	if (size_ <= SMALL_DATA_SIZE) {
		return;
	}

	// create sha signature
	sign_data(data_, size_, signature_);
	signed_ = true;
}

void
data_container::init() {
	// reset sha1 signature
	signed_ = false;
	memset(signature_, 0, SHA1_SIZE);

	// create ref counter
	std::string error_msg = "not enough memory to create new ref_counter in data container at ";
	error_msg += std::string(BOOST_CURRENT_FUNCTION);

	try {
		ref_counter_.reset(new reference_counter(0));
	}
	catch (...) {
		throw internal_error(error_msg);
	}

	if (!ref_counter_.get()) {
		throw internal_error(error_msg);
	}

	// init data
	data_ = NULL;
	size_ = 0;
}

data_container::~data_container() {
	release();
}

void
data_container::release() {
	if (!ref_counter_) {
		return;
	}

	if (*ref_counter_ == 0) {
		return;
	}

	--*ref_counter_;

	if (data_ && *ref_counter_ == 0) {
		delete [] data_;
		memset(&signature_, 0, SHA1_SIZE);
	}
}

data_container&
data_container::operator = (const data_container& rhs) {
	this->release();

	data_ = rhs.data_;
	size_ = rhs.size_;
	signed_ = rhs.signed_;

	if (rhs.signed_) {
		memcpy(signature_, rhs.signature_, SHA1_SIZE);
	}

	ref_counter_ = rhs.ref_counter_;
	++*ref_counter_;

	return *this;
}

bool
data_container::operator == (const data_container& rhs) const {
	// data size differs?
	if (size_ != rhs.size_) {
		return false;
	}

	// both containers empty?
	if (size_ == 0 && rhs.size_ == 0) {
		return true;
	}

	// compare small containers
	if (size_ <= SMALL_DATA_SIZE) {
		return (0 == memcmp(data_, rhs.data_, size_));
	}

	// compare big containers
	return (0 == memcmp(signature_, rhs.signature_, SHA1_SIZE));
}

bool
data_container::operator != (const data_container& rhs) const {
	return !(*this == rhs);
}

bool
data_container::empty() const {
	return (size_ == 0);
}

void
data_container::clear() {
	release();
	init();
}

void*
data_container::data() const {
	return (void*)data_;
}

size_t
data_container::size() const {
	return size_;
}

void
data_container::sign_data(unsigned char* data, size_t& size, unsigned char signature[SHA1_SIZE]) {
	SHA_CTX sha_context;
	SHA1_Init(&sha_context);

	// reset signature
	memset(signature, 0, SHA1_SIZE);

	// go through compiled and flattened data in chunks of 512 kb., building signature
	unsigned int full_chunks_count = size / SHA1_CHUNK_SIZE;
	char* data_offset_ptr = (char*)data;

	// process full chunks
	for (unsigned i = 0; i < full_chunks_count; i++) {
		SHA1_Update(&sha_context, data_offset_ptr, SHA1_CHUNK_SIZE);
		data_offset_ptr += SHA1_CHUNK_SIZE;
	}

	unsigned int remainder_chunk_size = 0;

	if (size < SHA1_CHUNK_SIZE) {
		remainder_chunk_size = size;
	}
	else {
		remainder_chunk_size = size % SHA1_CHUNK_SIZE;
	}

	// process remained chunk
	if (0 != remainder_chunk_size) {
		SHA1_Update(&sha_context, data_offset_ptr, remainder_chunk_size);
	}

	SHA1_Final(signature, &sha_context);
}

bool
data_container::is_data_loaded() {
	return true;
}

void
data_container::load_data() {
}

void
data_container::unload_data() {
}

void
data_container::remove_from_persistent_cache() {
}

} // namespace dealer
} // namespace cocaine
