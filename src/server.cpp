/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"
#include <rask/server.hpp>
#include <rask/workers.hpp>

#include <fost/internet>
#include <fost/log>


namespace {
    struct state {
        fostlib::json config;
        boost::asio::ip::tcp::acceptor listener;
    };

    void accept(rask::workers &w, std::shared_ptr<state> port) {
        auto socket = std::make_shared<rask::connection>(w);
        port->listener.async_accept(socket->cnx,
            [&w, port, socket](const boost::system::error_code &error ) {
                accept(w, port);
                if ( error ) {
                    fostlib::log::error("Server accept", error.message().c_str(), port->config);
                } else {
                    fostlib::log::info("Server accept", port->config);
                    monitor_connection(socket);
                    read_and_process(socket);
                }
            });
    }
}


void rask::listen(workers &w, const fostlib::json &config) {
    fostlib::host h(fostlib::coerce<fostlib::string>(config["bind"]));
    boost::asio::ip::tcp::endpoint endpoint{
        h.address(), fostlib::coerce<uint16_t>(config["port"])};
    std::shared_ptr<state> port{new state{
        config,
        {w.low_latency.io_service, endpoint}}};
    accept(w, port);
    fostlib::log::info("Rask now listening for peer connections", config);
}

