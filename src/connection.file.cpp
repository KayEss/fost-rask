/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/connection.hpp>
#include <rask/tenant.hpp>

#include <fost/counter>


namespace {
    fostlib::performance p_file_exists_received(
        rask::c_fost_rask, "packets", "file_exists", "received");
}


rask::connection::out rask::file_exists_out(
    tenant &t, const tick &p, const fostlib::string &n, const fostlib::json &
) {
    connection::out packet(0x90);
    packet << p << t.name() << n;
    return std::move(packet);
}


void rask::file_exists(rask::connection::in &packet) {
    ++p_file_exists_received;
    auto logger(fostlib::log::info(c_fost_rask));
    logger("", "File exists");
    auto priority(packet.read<tick>());
    logger("priority", priority);
    auto tenant(
        known_tenant(packet.socket->workers, packet.read<fostlib::string>()));
    auto name(packet.read<fostlib::string>());
    logger
        ("tenant", tenant->name())
        ("name", name);
    if ( tenant->subscription ) {
    }
}

