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

#include <boost/endian/conversion.hpp>


void rask::send_version(std::shared_ptr<connection> socket) {
    auto sender = socket->sender.wrap(
        [socket](const boost::system::error_code& error, std::size_t bytes) {
            if ( error ) {
                fostlib::log::error()
                    ("", "Version block not sent")
                    ("connection", socket->id)
                    ("error", error.message().c_str());
            } else {
                fostlib::log::debug()
                    ("", "Version block sent")
                    ("connection", socket->id)
                    ("version", int(rask::known_version))
                    ("bytes", bytes);
                socket->heartbeat.expires_from_now(boost::posix_time::seconds(5));
                socket->heartbeat.async_wait(
                    [socket](const boost::system::error_code &) {
                        send_version(socket);
                    });
            }
        });
    if ( rask::server_identity() ) {
        rask::tick time(rask::tick::now());
        std::array<unsigned char, 3 + 8 + 4> data{
            data.size() - 2, 0x80, rask::known_version};
        int64_t tick = boost::endian::native_to_big(time.time);
        int32_t server = boost::endian::native_to_big(time.server);
        std::memcpy(data.data() + 3, &tick, 8);
        std::memcpy(data.data() + 3 + 8, &server, 4);
        async_write(socket->cnx, boost::asio::buffer(data), sender);
    } else {
        static unsigned char data[] = {0x01, 0x80, rask::known_version};
        async_write(socket->cnx, boost::asio::buffer(data), sender);
    }
}


void rask::receive_version(std::shared_ptr<connection> socket, std::size_t packet_size) {
        const int version = socket->input_buffer.sbumpc();
        int64_t time = 0; int32_t server = 0;
        if ( --packet_size ) {
            socket->input_buffer.sgetn(reinterpret_cast<char*>(&time), 8);
            time = boost::endian::big_to_native(time);
            socket->input_buffer.sgetn(reinterpret_cast<char*>(&server), 4);
            server = boost::endian::big_to_native(server);
            tick::overheard(time, server);
            packet_size -= 12;
            while ( packet_size-- ) socket->input_buffer.sbumpc();
        }
        fostlib::log::info()
            ("", "Version block")
            ("connection", socket->id)
            ("version", version)
            ("tick", "time", time)
            ("tick", "server", server);
}

