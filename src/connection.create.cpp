/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"
#include <rask/tenant.hpp>


rask::connection::out rask::create_directory(
    rask::tenant &tenant, const rask::tick &priority,
    fostlib::jsondb::local &, const fostlib::jcursor &,
    const fostlib::string &name
) {
    connection::out packet(0x91);
    packet << priority << tenant.name() << name;
    return std::move(packet);
}


void rask::create_directory(rask::connection::in &packet) {
    auto logger(fostlib::log::info());
    logger("", "Create directory");
    auto priority(packet.read<tick>());
    logger("priority", priority);
    auto tenant(known_tenant(packet.read<fostlib::string>()));
    auto name(packet.read<fostlib::string>());
    if ( !tenant->local_path.empty() ) {
        boost::filesystem::create_directories(
            tenant->local_path / fostlib::coerce<boost::filesystem::path>(name));
    }
    logger
        ("tenant", tenant->name())
        ("name", name);
}

