/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include <rask/connection.hpp>
#include <rask/workers.hpp>


rask::connection::out rask::tenant_packet(
    const fostlib::string &name, const fostlib::json &meta
) {
    connection::out packet(0x81);
    packet << name;
    const auto hash = fostlib::coerce<std::vector<unsigned char>>(
            fostlib::base64_string(
                fostlib::coerce<fostlib::ascii_string>(
                    fostlib::coerce<fostlib::string>(meta["hash"]["data"]))));
    packet << hash;
    return std::move(packet);
}


void rask::tenant_packet(connection::in &packet) {
    auto logger(fostlib::log::info(c_fost_rask));
    logger
        ("", "Tenant packet")
        ("connection", packet.socket_id());
    auto name(packet.read<fostlib::string>());
    logger("name", name);
    auto hash(packet.read(32));
    auto hash64 = fostlib::coerce<fostlib::base64_string>(hash);
    logger("hash",  hash64.underlying().underlying().c_str());
    packet.socket->workers.high_latency.get_io_service().post(
        [socket = packet.socket, name = std::move(name), hash = std::move(hash)]() {
            if ( socket->identity ) {
                auto partner(peer::server(socket->identity));
                auto tenant(partner->tenants.add_if_not_found(name,
                    [](){ return std::make_shared<peer::tenant>(); }));
                std::array<unsigned char, 32> hash_array;
                std::copy(hash.begin(), hash.end(), hash_array.begin());
                tenant->hash = hash_array;
            }
        });
}

