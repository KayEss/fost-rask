/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include <rask/workers.hpp>

#include <fost/log>

#include <boost/asio/spawn.hpp>

#include <mutex>


namespace {
    std::mutex g_mutex;
    std::vector<std::weak_ptr<rask::connection>> g_connections;
}


void rask::monitor_connection(std::shared_ptr<rask::connection> socket) {
    send_version(socket);
    std::unique_lock<std::mutex> lock(g_mutex);
    for ( auto w = g_connections.begin(); w != g_connections.end(); ++w ) {
        std::shared_ptr<rask::connection> slot(w->lock());
        if ( !slot ) {
            *w = socket;
            return;
        }
    }
    g_connections.push_back(socket);
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
                    receive_version(socket, packet_size);
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


/*
    rask::connection::out
*/


rask::connection::out &rask::connection::out::size_sequence(
    std::size_t s, boost::asio::streambuf &b
) {
    if ( s < 0x80 ) {
        b.sputc(s);
    } else {
        throw fostlib::exceptions::not_implemented(
            "Large packet sizes");
    }
    return *this;
}

