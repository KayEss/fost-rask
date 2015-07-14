/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/configuration.hpp>
#include <rask/connection.hpp>
#include <rask/server.hpp>
#include <rask/tenant.hpp>
#include <rask/workers.hpp>

#include <beanbag/beanbag>
#include <fost/internet>
#include <fost/log>


namespace {
    std::atomic<uint32_t> g_identity;

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
                    fostlib::log::error(rask::c_fost_rask,
                        "Server accept", error.message().c_str(), port->config);
                } else {
                    fostlib::log::info(rask::c_fost_rask,
                        "Server accept", port->config);
                    monitor_connection(socket);
                    read_and_process(socket);
                }
            });
    }
}


uint32_t rask::server_identity() {
    return g_identity;
}


void rask::server(workers &w) {
    if ( !c_server_db.value().isnull() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_server_db.value()["database"]));
        fostlib::jsondb::local server(*dbp);
        if ( !server.has_key("identity") ) {
            std::ifstream urandom("/dev/urandom");
            uint32_t random = unsigned(urandom.get()) << 24u;
            random += unsigned(urandom.get()) << 16u;
            random += unsigned(urandom.get()) << 8u;
            random += unsigned(urandom.get());
            server.set("identity", random);
            server.commit();
            fostlib::log::info(c_fost_rask)("Server identity picked as", random);
            g_identity = random;
        } else {
            g_identity = fostlib::coerce<uint32_t>(server["identity"]);
        }
        // Start listening for connections
        rask::listen(w, c_server_db.value()["socket"]);
        // Load tenants and start sweeping
        if ( !c_tenant_db.value().isnull() || !c_subscriptions_db.value().isnull() ) {
            rask::tenants(w, c_tenant_db.value(), c_subscriptions_db.value());
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
        {w.low_latency.get_io_service(), endpoint}}};
    accept(w, port);
    fostlib::log::info(c_fost_rask, "Rask now listening for peer connections", config);
}

