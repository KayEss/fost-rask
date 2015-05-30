/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include <rask/peer.hpp>
#include <rask/workers.hpp>

#include <fost/internet>
#include <fost/log>
#include <beanbag/beanbag>


void rask::peer(workers &w, const fostlib::json &dbconf) {
    fostlib::log::debug("Starting peering", dbconf);
    beanbag::jsondb_ptr dbp(beanbag::database(dbconf));
    auto configure = [&w, dbp](const fostlib::json &peers) {
        if ( peers.has_key("connect") ) {
            const fostlib::json connect(peers["connect"]);
            for ( auto c(connect.begin()); c != connect.end(); ++c ) {
                auto connect = std::make_shared<connection::reconnect>(w, *c);
                peer_with(connect);
            }
        }
    };
    dbp->post_commit(configure);
    fostlib::jsondb::local db(*dbp);
    configure(db.data());
}


void rask::peer_with(std::shared_ptr<connection::reconnect> client) {
    fostlib::log::debug("About to try to connect to", client->configuration);
    auto socket = std::make_shared<rask::connection>(client->watchdog.get_io_service());
    client->socket = socket;
    client->watchdog.expires_from_now(boost::posix_time::seconds(15));
    client->watchdog.async_wait(
        [client](const boost::system::error_code &error) {
            std::shared_ptr<connection> socket(client->socket);
            if ( socket ) {
                socket->cnx.cancel();
                socket->cnx.close();
            }
            fostlib::log::error()
                ("", "Watchdog timer fired")
                ("error", error.message().c_str())
                ("peer", client->configuration);
            peer_with(client);
        });
    boost::asio::ip::tcp::resolver resolver(client->watchdog.get_io_service());
    boost::asio::ip::tcp::resolver::query q(
        fostlib::coerce<fostlib::string>(client->configuration["host"]).c_str(),
        fostlib::coerce<fostlib::string>(client->configuration["port"]).c_str());
    boost::asio::async_connect(socket->cnx, resolver.resolve(q),
        [socket](const boost::system::error_code &error, auto iterator) {
            if ( error ) {
                fostlib::log::error("Connected to peer", error.message().c_str());
            } else {
                fostlib::log::debug("Connected to peer");
                monitor_connection(socket);
                read_and_process(socket);
            }
        });
}

