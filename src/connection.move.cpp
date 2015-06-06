/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/tenant.hpp>


rask::connection::out rask::move_out(
    rask::tenant &tenant, const rask::tick &priority,
    fostlib::jsondb::local &, const fostlib::jcursor &,
    const fostlib::string &name
) {
    connection::out packet(0x93);
    packet << priority << tenant.name() << name;
    return std::move(packet);
}


void rask::move_out(rask::connection::in &packet) {
    auto logger(fostlib::log::info());
    logger("", "Remove inode");
    auto priority(packet.read<tick>());
    logger("priority", priority);
    auto tenant(known_tenant(packet.read<fostlib::string>()));
    auto name(packet.read<fostlib::string>());
    logger
        ("tenant", tenant->name())
        ("name", name);
}

