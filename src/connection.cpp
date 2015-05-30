/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"
#include <rask/workers.hpp>

#include <fost/log>

#include <boost/asio/spawn.hpp>

#include <mutex>


namespace {
    std::mutex g_mutex;
    std::vector<std::weak_ptr<rask::connection>> g_connections;
}


std::atomic<int64_t> rask::connection::g_id(0);


rask::connection::connection(workers &w)
: id(++g_id), cnx(w.low_latency.io_service), sender(w.low_latency.io_service),
        heartbeat(w.low_latency.io_service) {
}


rask::connection::~connection() {
    fostlib::log::debug("Connection closed", id);
}


void rask::monitor_connection(std::shared_ptr<rask::connection> socket) {
    static unsigned char data[] = {0x01, 0x80, 0x01};
    async_write(socket->cnx, boost::asio::buffer(data), socket->sender.wrap(
        [socket](const boost::system::error_code& error, std::size_t bytes) {
            if ( error ) {
                fostlib::log::error()
                    ("", "Version block not sent")
                    ("error", error.message().c_str());
            } else {
                fostlib::log::debug()
                    ("", "Version block sent")
                    ("version", int(data[2]))
                    ("bytes", bytes);
                socket->heartbeat.expires_from_now(boost::posix_time::seconds(5));
                socket->heartbeat.async_wait(
                    [socket](const boost::system::error_code&) {
                        monitor_connection(socket);
                    });
            }
        }));
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
                    fostlib::log::info()
                        ("", "Version block")
                        ("connection", socket->id)
                        ("version", int(socket->input_buffer.sbumpc()));
                } else {
                    fostlib::log::warning()
                        ("", "Unknown control byte received")
                        ("connection", socket->id)
                        ("control", int(control))
                        ("packet-size", packet_size);
                        while ( packet_size-- ) socket->input_buffer.sbumpc();
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

