/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"
#include <rask/tenants.hpp>


rask::connection::out rask::create_directory(
    rask::tenant &, const rask::tick &,
    fostlib::jsondb::local &, const fostlib::jcursor &,
    const fostlib::string &name
) {
    connection::out packet(0x91);
    return std::move(packet);
}

