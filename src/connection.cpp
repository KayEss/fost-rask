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


rask::connection::connection(workers &w)
: cnx(w.low_latency.io_service), sender(w.low_latency.io_service) {
}


void rask::connection::version() {
    static unsigned char data[] = {0x01, 0x80, 0x01};
    async_write(cnx, boost::asio::buffer(data), sender.wrap(
        [](const boost::system::error_code& error, std::size_t bytes) {
            if ( error ) {
                fostlib::log::error()
                    ("", "Version block not sent")
                    ("error", error.message().c_str());
            } else {
                fostlib::log::debug()
                    ("", "Version block sent")
                    ("version", int(data[2]))
                    ("bytes", bytes);
            }
        }));
}


void rask::read_and_process(std::shared_ptr<rask::connection> socket) {
    boost::asio::spawn(socket->cnx.get_io_service(),
        [socket](boost::asio::yield_context yield) {
            boost::system::error_code error;
            std::size_t packet_size;
            unsigned char bytes[2];
            boost::asio::async_read(socket->cnx, boost::asio::buffer(bytes), yield[error]);
            if ( !error ) {
                if ( bytes[0] < 0x80 ) {
                    packet_size = bytes[0];
                    fostlib::log::debug("Got packet of size", packet_size);
                } else {
                    throw fostlib::exceptions::not_implemented(
                        "Large packets are not implemented");
                }
            }
        });
}

