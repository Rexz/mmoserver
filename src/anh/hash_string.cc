/*
 This file is part of MMOServer. For more information, visit http://swganh.com
 
 Copyright (c) 2006 - 2014 The SWG:ANH Team

 MMOServer is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 MMOServer is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with MMOServer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "anh/hash_string.h"
#include "anh/crc.h"

using namespace swganh;

HashString::HashString(const char* ident_string)
    : ident_(reinterpret_cast<void*>(memcrc(std::string(ident_string))))
    , ident_string_(ident_string)
{}

HashString::HashString(const std::string &std_string)
    : ident_(reinterpret_cast<void*>(memcrc(std_string)))
    , ident_string_(std_string)
{}

HashString::~HashString() {}

HashString::HashString(const HashString& other) {
    ident_ = other.ident_;
    ident_string_ = other.ident_string_;
}

HashString::HashString(HashString&& other) {
    ident_ = other.ident_;
    ident_string_ = std::move(other.ident_string_);
}

void HashString::swap(HashString& other) {
    std::swap(other.ident_, ident_);
    std::swap(other.ident_string_, ident_string_);
}

HashString& HashString::operator=(HashString other) {
    other.swap(*this);
    return *this;
}

HashString::operator uint32_t () const {
    return ident();
}

bool HashString::operator<(const HashString& other) const {
    bool r = (ident() < other.ident());
    return r;
}

bool HashString::operator>(const HashString& other) const {
    bool r = (ident() > other.ident());
    return r;
}

bool HashString::operator==(const HashString& other) const {
    bool r = (ident() == other.ident());
    return r;
}

bool HashString::operator!=(const HashString& other) const {
    bool r = (ident() != other.ident());
    return r;
}

uint32_t HashString::ident() const {
    return static_cast<uint32_t>(reinterpret_cast<uint64_t>(ident_));
}

const std::string& HashString::ident_string() const {
    return ident_string_;
}
