/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include <rask/clock.hpp>
#include <rask/configuration.hpp>
#include <rask/workers.hpp>

#include <fost/counter>
#include <fost/log>

#include <boost/asio/spawn.hpp>

#include <mutex>


namespace {
    std::mutex g_mutex;
    std::vector<std::weak_ptr<rask::connection>> g_connections;

    fostlib::performance p_queued(rask::c_fost_rask, "packets", "queued");
    fostlib::performance p_sends(rask::c_fost_rask, "packets", "sends");
    fostlib::performance p_spill(rask::c_fost_rask, "packets", "spills");
    fostlib::performance p_received(rask::c_fost_rask, "packets", "received");
    fostlib::performance p_processed(rask::c_fost_rask, "packets", "processed");
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
                fostlib::json size_bytes = fostlib::json::array_t();
                boost::asio::async_read(socket->cnx, socket->input_buffer,
                    boost::asio::transfer_exactly(2), yield);
                std::size_t packet_size = socket->input_buffer.sbumpc();
                fostlib::push_back(size_bytes, int64_t(packet_size));
                if ( packet_size > 0xf8 ) {
                    const int bytes = packet_size - 0xf8;
                    boost::asio::async_read(socket->cnx, socket->input_buffer,
                        boost::asio::transfer_exactly(bytes), yield);
                    packet_size = 0u;
                    for ( auto i = 0; i != bytes; ++i ) {
                        unsigned char byte = socket->input_buffer.sbumpc();
                        fostlib::push_back(size_bytes, int64_t(byte));
                        packet_size = (packet_size << 8) + byte;
                    }
                } else if ( packet_size >= 0x80 ) {
                    socket->cnx.close();
                    throw fostlib::exceptions::not_implemented(
                        "Invalid packet size control byte",
                        fostlib::coerce<fostlib::string>(packet_size));
                }
                unsigned char control = socket->input_buffer.sbumpc();
                boost::asio::async_read(socket->cnx, socket->input_buffer,
                    boost::asio::transfer_exactly(packet_size), yield);
                fostlib::log::debug(c_fost_rask)
                    ("", "Got packet")
                    ("connection", socket->id)
                    ("bytes", size_bytes)
                    ("control", control)
                    ("size", packet_size);
                connection::in packet(socket, packet_size);
                if ( control == 0x80 ) {
                    receive_version(packet);
                } else if ( control == 0x81 ) {
                    tenant_packet(packet);
                } else if ( control == 0x82 ) {
                    tenant_hash_packet(packet);
                } else if ( control == 0x83 ) {
                    file_hash_without_priority(packet);
                } else if ( control == 0x90 ) {
                    file_exists(packet);
                } else if ( control == 0x91 ) {
                    create_directory(packet);
                } else if ( control == 0x93 ) {
                    move_out(packet);
                } else if ( control == 0x9f ) {
                    file_data_block(packet);
                } else {
                    fostlib::log::warning(c_fost_rask)
                        ("", "Unknown control byte received")
                        ("connection", socket->id)
                        ("control", int(control))
                        ("packet-size", packet_size);
                }
                if ( socket->restart ) {
                    reset_watchdog(socket->workers, socket->restart);
                }
                read_and_process(socket);
            } catch ( std::exception &e ) {
                fostlib::log::error(c_fost_rask)
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


const std::size_t queue_capactiy = 256;


std::atomic<int64_t> rask::connection::g_id(0);


rask::connection::connection(rask::workers &w)
: workers(w), id(++g_id), cnx(w.io.get_io_service()),
        sender(w.io.get_io_service()),
        heartbeat(w.io.get_io_service()),
        identity(0), packets(queue_capactiy) {
    input_buffer.prepare(buffer_size);
}


rask::connection::~connection() {
    fostlib::log::debug(c_fost_rask, "Connection closed", id);
}


void rask::connection::queue(std::function<out(void)> fn) {
    bool added = false;
    const auto size = packets.push_back(
        [fn]() {
            ++p_queued;
            added = true;
            return fn;
        },
        [](auto &) {
            ++p_spill;
            return false;
        });
    /// We notify the consumer here and not in the lambda above because
    /// when that lambda executes the function is not yet in the buffer so
    /// there would be race between getting it there and the consumer
    /// pulling it off. Because the queue is protected by a mutex this
    /// can't actually be a problem, until the queue is re-implemented
    /// to be lock free, and then it will be. Doing it at the end is always
    /// safe.
    if ( added ) {
        sender.produced();
    }
}


void rask::connection::send_head() {
    auto self(shared_from_this());
    auto fn = packets.pop_front(
        fostlib::nullable<std::function<out(void)>>());
    if ( !fn.isnull() ) {
        ++p_sends;
        fn.value()()(self,
            [self]() {
                self->send_head();
            });
    }
}


/*
    rask::connection::reconnect
*/


rask::connection::reconnect::reconnect(rask::workers &w, const fostlib::json &conf)
: configuration(conf), watchdog(w.io.get_io_service()) {
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


rask::connection::out &rask::connection::out::operator << (
    const std::vector<unsigned char> &v
) {
    if ( v.size() ) {
        buffer->sputn(reinterpret_cast<const char *>(v.data()), v.size());
    }
    return *this;
}


void rask::connection::out::size_sequence(std::size_t s, boost::asio::streambuf &b) {
    if ( s < 0x80 ) {
        b.sputc(s);
    } else if ( s < 0x100 ) {
        b.sputc(0xf9);
        b.sputc(s);
    } else if ( s < 0x10000 ) {
        b.sputc(0xfa);
        b.sputc(s >> 8);
        b.sputc(s & 0xff);
    } else {
        throw fostlib::exceptions::not_implemented(
            "Large packet sizes", fostlib::coerce<fostlib::string>(s));
    }
}


/*
    rask::connection::in
*/


rask::connection::in::in(std::shared_ptr<connection> socket, std::size_t s)
: socket(socket), remaining(s) {
    ++p_received;
}


rask::connection::in::~in() {
     while ( remaining-- ) socket->input_buffer.sbumpc();
     ++p_processed;
}


void rask::connection::in::check(std::size_t b) const {
    if ( remaining < b )
        throw fostlib::exceptions::unexpected_eof(
            "Not enough data in the buffer for this packet");
}


std::size_t rask::connection::in::size_control() {
    auto header(read<uint8_t>());
    if ( header < 0x80 ) {
        return header;
    } else if ( header > 0xf8 ) {
        std::size_t bytes = 0;
        /// We disallow anything too big
        while ( header > 0xf8 && header <= 0xfa ) {
            bytes *= 0x100;
            bytes += read<unsigned char>();
            --header;
        }
        return bytes;
    } else
        throw fostlib::exceptions::not_implemented(
            "size_control recived invalid size byte",
            fostlib::coerce<fostlib::string>(int(header)));
}


std::vector<unsigned char> rask::connection::in::read(std::size_t b) {
    check(b);
    std::vector<unsigned char> data(b, 0);
    socket->input_buffer.sgetn(reinterpret_cast<char *>(data.data()), b);
    remaining -= b;
    return data;
}

