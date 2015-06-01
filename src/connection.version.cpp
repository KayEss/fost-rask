/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"
#include <rask/clock.hpp>
#include <rask/server.hpp>

#include <fost/log>


void rask::send_version(std::shared_ptr<connection> socket) {
    connection::out version(0x80);
    version << rask::known_version;
    if ( rask::server_identity() ) {
        version << rask::tick::now();
    }
    version(socket, [socket]() {
        socket->heartbeat.expires_from_now(boost::posix_time::seconds(5));
        socket->heartbeat.async_wait(
            [socket](const boost::system::error_code &) {
                send_version(socket);
            });
    });
}


void rask::receive_version(std::shared_ptr<connection> socket, std::size_t packet_size) {
    auto logger(fostlib::log::info());
    logger
        ("", "Version block")
        ("connection", socket->id);
    const int version = socket->input_buffer.sbumpc();
    logger
        ("version", version);
    int64_t time = 0; int32_t server = 0;
    if ( --packet_size ) {
        int64_t ptime; int32_t pserver;
        socket->input_buffer.sgetn(reinterpret_cast<char*>(&ptime), 8);
        time = boost::endian::big_to_native(ptime);
        socket->input_buffer.sgetn(reinterpret_cast<char*>(&pserver), 4);
        server = boost::endian::big_to_native(pserver);
        tick::overheard(time, server);
        packet_size -= 12;
        logger
            ("tick", "time", time)
            ("tick", "server", server);
        while ( packet_size-- ) socket->input_buffer.sbumpc();
    }
}

