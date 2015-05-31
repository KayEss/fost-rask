/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include <rask/clock.hpp>
#include <rask/server.hpp>
#include <rask/workers.hpp>

#include <fost/log>

#include <boost/asio/spawn.hpp>

#include <mutex>


namespace {
    std::mutex g_mutex;
    std::vector<std::weak_ptr<rask::connection>> g_connections;
}


void rask::monitor_connection(std::shared_ptr<rask::connection> socket) {
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
                    ("version", int(known_version))
                    ("bytes", bytes);
                socket->heartbeat.expires_from_now(boost::posix_time::seconds(5));
                socket->heartbeat.async_wait(
                    [socket](const boost::system::error_code &) {
                        monitor_connection(socket);
                    });
            }
        });
    if ( server_identity() ) {
        tick time(tick::now());
        std::array<unsigned char, 3 + 8 + 4> data;
        data[0] = data.size() - 2;
        data[1] = 0x80;
        data[2] = known_version;
        std::memcpy(data.data() + 3, &time.time, 8);
        std::memcpy(data.data() + 3 + 8, &time.server, 4);
        async_write(socket->cnx, boost::asio::buffer(data), sender);
    } else {
        static unsigned char data[] = {0x01, 0x80, known_version};
        async_write(socket->cnx, boost::asio::buffer(data), sender);
    }
}


void rask::read_and_process(std::shared_ptr<rask::connection> socket) {
    boost::asio::spawn(socket->cnx.get_io_service(),
        [socket](boost::asio::yield_context yield) {
            try {
                boost::asio::async_read(socket->cnx, socket->input_buffer,
                    boost::asio::transfer_exactly(2), yield);
                std::size_t packet_size = socket->input_buffer.sbumpc();
                if ( packet_size < 0x80 ) {
                    fostlib::log::debug()
                        ("", "Got packet of size")
                        ("connection", socket->id)
                        ("size", packet_size);
                } else {
                    throw fostlib::exceptions::not_implemented(
                        "Large packets are not implemented");
                }
                unsigned char control = socket->input_buffer.sbumpc();
                boost::asio::async_read(socket->cnx, socket->input_buffer,
                    boost::asio::transfer_exactly(packet_size), yield);
                if ( control == 0x80 ) {
                    const int version = socket->input_buffer.sbumpc();
                    int64_t time = 0; int32_t server = 0;
                    if ( --packet_size ) {
                        socket->input_buffer.sgetn(reinterpret_cast<char*>(&time), 8);
                        socket->input_buffer.sgetn(reinterpret_cast<char*>(&server), 4);
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
                } else {
                    fostlib::log::warning()
                        ("", "Unknown control byte received")
                        ("connection", socket->id)
                        ("control", int(control))
                        ("packet-size", packet_size);
                        while ( packet_size-- ) socket->input_buffer.sbumpc();
                }
                if ( socket->restart ) {
                    reset_watchdog(socket->restart);
                }
                read_and_process(socket);
            } catch ( std::exception &e ) {
                fostlib::log::error()
                    ("", "read_and_process caught an exception")
                    ("connection", socket->id)
                    ("exception", e.what());
                fostlib::absorb_exception();
            }
        });
}


/*
    rask::connection
*/


std::atomic<int64_t> rask::connection::g_id(0);


rask::connection::connection(boost::asio::io_service &service)
: id(++g_id), cnx(service), sender(service), heartbeat(service) {
}


rask::connection::~connection() {
    fostlib::log::debug("Connection closed", id);
}


/*
    rask::connection::reconnect
*/


rask::connection::reconnect::reconnect(workers &w, const fostlib::json &conf)
: configuration(conf), watchdog(w.low_latency.io_service) {
}

