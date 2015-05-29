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
                fostlib::log::debug("About to connect to", *c);
                boost::asio::ip::tcp::resolver resolver(w.low_latency.io_service);
                boost::asio::ip::tcp::resolver::query q(
                    fostlib::coerce<fostlib::string>((*c)["host"]).c_str(),
                    fostlib::coerce<fostlib::string>((*c)["port"]).c_str());
                std::shared_ptr<rask::connection> socket(new rask::connection{
                    boost::asio::ip::tcp::socket(w.low_latency.io_service)});
                boost::asio::async_connect(socket->cnx, resolver.resolve(q),
                    [&w, socket](const boost::system::error_code& error, auto iterator) {
                        fostlib::log::debug("Connected to peer");
                    });
            }
        }
    };
    dbp->post_commit(configure);
    fostlib::jsondb::local db(*dbp);
    configure(db.data());
}

