/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"
#include <rask/configuration.hpp>
#include <rask/server.hpp>
#include <rask/tenants.hpp>
#include <rask/workers.hpp>

#include <beanbag/beanbag>
#include <fost/internet>
#include <fost/log>


namespace {
    struct state {
        fostlib::json config;
        boost::asio::ip::tcp::acceptor listener;
    };

    void accept(rask::workers &w, std::shared_ptr<state> port) {
        auto socket = std::make_shared<rask::connection>(w.low_latency.io_service);
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


void rask::server(workers &w) {
    if ( !c_server_db.value().isnull() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_server_db.value()["database"]));
        fostlib::jsondb::local server(*dbp);
        if ( !server.has_key("identity") ) {
            uint32_t random = 0;
            std::ifstream urandom("/dev/urandom");
            random += urandom.get() << 16;
            random += urandom.get() << 8;
            random += urandom.get();
            random &= (1 << 20) - 1; // Take 20 bits
            server.set("identity", random);
            server.commit();
            fostlib::log::info()("Server identity picked as", random);
        }
        // Start listening for connections
        rask::listen(w, c_server_db.value()["socket"]);
        // Load tenants and start sweeping
        if ( !c_tenant_db.value().isnull() ) {
            rask::tenants(w, rask::c_tenant_db.value());
            w.notify();
        }
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

