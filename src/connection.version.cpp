/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include <rask/clock.hpp>
#include <rask/tenant.hpp>
#include <rask/server.hpp>
#include <rask/workers.hpp>

#include <beanbag/beanbag>

#include <fost/counter>
#include <fost/log>


namespace {
    fostlib::performance p_received(rask::c_fost_rask,
        "packets", "version", "received");
}


rask::connection::out rask::send_version() {
    connection::out version(0x80);
    version << rask::known_version;
    if ( server_identity() ) {
        version << server_identity();
        auto state = rask::tick::now();
        version << state.first;
        if ( !state.second.isnull() ) {
            const auto hash = fostlib::coerce<std::vector<unsigned char>>(
                    fostlib::base64_string(
                        fostlib::coerce<fostlib::ascii_string>(
                            state.second.value())));
            version << hash;
        }
    }
    return std::move(version);
}


void rask::receive_version(connection::in &packet) {
    ++p_received;
    auto logger(fostlib::log::info(c_fost_rask));
    logger
        ("", "Version block")
        ("connection", packet.socket_id());
    const auto version(packet.read<int8_t>());
    logger("version", version);
    if ( !packet.empty() ) {
        auto identity(packet.read<uint32_t>());
        packet.socket->identity = identity;
        logger("peer", identity);
        auto time(packet.read<tick>());
        tick::overheard(time.time, time.server);
        logger("tick", time);
        if ( !packet.empty() ) {
            // We got a hash
            auto hash(packet.read(32));
            auto hash64 = fostlib::coerce<fostlib::base64_string>(hash);
            logger("hash",  hash64.underlying().underlying().c_str());
            // Now compare to see if we need to have a conversation about it
            auto myhash = tick::now();
            if ( !myhash.second.isnull() &&
                    myhash.second.value() != fostlib::coerce<fostlib::string>(hash64) ) {
                logger("conversation", "sending tenants");
                packet.socket->workers.responses.get_io_service().post(
                    [socket = packet.socket]() {
                        auto tdbconf = c_tenant_db.value();
                        if ( !tdbconf.isnull() ) {
                            auto tenants_dbp = beanbag::database(tdbconf);
                            fostlib::jsondb::local tenants(*tenants_dbp);
                            static const fostlib::jcursor pos("known");
                            for ( auto iter(tenants[pos].begin()); iter != tenants[pos].end(); ++iter ) {
                                auto name = fostlib::coerce<fostlib::string>(iter.key());
                                auto mytenant(known_tenant(socket->workers, name));
                                socket->queue(
                                    [name = std::move(name), data = *iter]() {
                                        return tenant_packet(name, data);
                                    });
                            }
                        }
                    });
            } else {
                logger("conversation", "not needed");
            }
        }
    }
}

