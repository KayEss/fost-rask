/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"
#include "peer.hpp"
#include <rask/workers.hpp>


rask::connection::out rask::tenant_packet(const fostlib::string &name) {
    connection::out packet(0x81);
    packet << name;
    return std::move(packet);
}


void rask::tenant_packet(connection::in &packet) {
    auto logger(fostlib::log::info());
    logger
        ("", "Tenant packet")
        ("connection", packet.socket_id());
    auto name(packet.read<fostlib::string>());
    logger("name", name);
    packet.socket->workers.high_latency.io_service.post(
        [name = std::move(name), socket = packet.socket]() {
            if ( socket->identity ) {
                peer &partner(peer::server(socket->identity));
                partner.tenants.emplace_if_not_found(name);
            }
        });
}

