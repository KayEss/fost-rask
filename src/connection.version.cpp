/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.conversation.hpp"
#include "peer.hpp"
#include <rask/clock.hpp>
#include <rask/tenant.hpp>
#include <rask/server.hpp>
#include <rask/workers.hpp>

#include <fost/log>


void rask::send_version(std::shared_ptr<connection> socket) {
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
    version(socket, [socket]() {
        socket->heartbeat.expires_from_now(boost::posix_time::seconds(5));
        socket->heartbeat.async_wait(
            [socket](const boost::system::error_code &) {
                send_version(socket);
            });
    });
}


void rask::receive_version(connection::in &packet) {
    auto logger(fostlib::log::info(c_fost_rask));
    logger
        ("", "Version block")
        ("connection", packet.socket_id());
    const auto version(packet.read<int8_t>());
    logger("version", version);
    if ( !packet.empty() ) {
        auto identity(packet.read<uint32_t>());
        packet.socket->identity = identity;
        std::shared_ptr<peer> server(peer::server(identity));
        logger("peer", identity);
        auto time(packet.read<tick>());
        tick::overheard(time.time, time.server);
        logger("tick", time);
        if ( !packet.empty() ) {
            // We got a hash
            auto hash(packet.read(32));
            auto hash64 = fostlib::coerce<fostlib::base64_string>(hash);
            logger("hash",  hash64.underlying().underlying().c_str());
            // Store it
            std::array<unsigned char, 32> hash_array;
            std::copy(hash.begin(), hash.end(), hash_array.begin());
            server->hash = hash_array;
            // Now compare to see if we need to have a conversation about it
            auto myhash = tick::now();
            if ( !myhash.second.isnull() &&
                    myhash.second.value() != fostlib::coerce<fostlib::string>(hash64) ) {
                logger("conversation", "sending tenants");
                packet.socket->workers.high_latency.get_io_service().post(
                    [socket = packet.socket]() {
                        auto tdbconf = c_tenant_db.value();
                        if ( !tdbconf.isnull() ) {
                            auto tenants_dbp = beanbag::database(tdbconf);
                            fostlib::jsondb::local tenants(*tenants_dbp);
                            fostlib::jcursor pos("known");
                            for ( auto iter(tenants[pos].begin()); iter != tenants[pos].end(); ++iter ) {
                                auto name = fostlib::coerce<fostlib::string>(iter.key());
                                auto partner = peer::server(socket->identity);
                                auto ptenant = partner->tenants.find(name);
                                std::shared_ptr<tenant> mytenant(known_tenant(name));
                                if ( !ptenant || mytenant->hash.load() != ptenant->hash.load() ) {
                                    socket->queue(
                                        [name = std::move(name), data = *iter]() {
                                            return tenant_packet(name, data);
                                        });
                                }
                            }
                        }
                    });
            } else {
                logger("conversation", "not needed");
            }
        }
    }
}

