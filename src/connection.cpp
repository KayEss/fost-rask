/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include <rask/clock.hpp>
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


std::size_t rask::broadcast(const connection::out &packet) {
    std::size_t to = 0;
    std::unique_lock<std::mutex> lock(g_mutex);
    for ( auto w = g_connections.begin(); w != g_connections.end(); ++w ) {
        std::shared_ptr<rask::connection> slot(w->lock());
        if ( slot ) {
            ++to;
            packet(slot);
        }
    }
    return to;
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
                connection::in packet(socket, packet_size);
                if ( control == 0x80 ) {
                    receive_version(packet);
                } else if ( control == 0x91 ) {
                    create_directory(packet);
                } else {
                    fostlib::log::warning()
                        ("", "Unknown control byte received")
                        ("connection", socket->id)
                        ("control", int(control))
                        ("packet-size", packet_size);
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


rask::connection::out &rask::connection::out::operator << (const tick &t) {
    return (*this) << t.time << t.server;
}


rask::connection::out &rask::connection::out::operator << (
    const fostlib::string &s
) {
    // This implementation only works for narrow character string
    return size_sequence(s.native_length()) <<
        fostlib::const_memory_block(s.c_str(), s.c_str() + s.native_length());
}


rask::connection::out &rask::connection::out::operator << (
    const fostlib::const_memory_block b
) {
    if ( b.first != b.second )
        buffer->sputn(reinterpret_cast<const char *>(b.first),
            reinterpret_cast<const char*>(b.second)
                - reinterpret_cast<const char*>(b.first));
    return *this;
}


void rask::connection::out::size_sequence(std::size_t s, boost::asio::streambuf &b) {
    if ( s < 0x80 ) {
        b.sputc(s);
    } else {
        throw fostlib::exceptions::not_implemented(
            "Large packet sizes", fostlib::coerce<fostlib::string>(s));
    }
}


/*
    rask::connection::in
*/


rask::connection::in::~in() {
     while ( remaining-- ) socket->input_buffer.sbumpc();
}


void rask::connection::in::check(std::size_t b) const {
    if ( remaining < b )
        throw fostlib::exceptions::unexpected_eof(
            "Not enough data in the buffer for this packet");
}


std::size_t rask::connection::in::size_control() {
    auto header(read<uint8_t>());
    if ( header < 0x80 )
        return header;
    else
        throw fostlib::exceptions::not_implemented(
            "Large data blocks embedded in a packet");
}


std::vector<unsigned char> rask::connection::in::read(std::size_t b) {
    check(b);
    std::vector<unsigned char> data(b, 0);
    socket->input_buffer.sgetn(reinterpret_cast<char *>(data.data()), b);
    remaining -= b;
    return data;
}

