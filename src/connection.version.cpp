/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"
#include <rask/clock.hpp>
#include <rask/server.hpp>

#include <fost/log>


void rask::send_version(std::shared_ptr<connection> socket) {
    connection::out version(0x80);
    version << rask::known_version;
    if ( rask::server_identity() ) {
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
    auto logger(fostlib::log::info());
    logger
        ("", "Version block")
        ("connection", packet.socket_id());
    const auto version(packet.read<int8_t>());
    logger("version", version);
    if ( !packet.empty() ) {
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
            packet.socket->hash = hash_array;
            // Now compare to see if we need to have a conversation about it
            auto myhash = tick::now();
            if ( !myhash.second.isnull() &&
                    myhash.second.value() != fostlib::coerce<fostlib::string>(hash64) ) {
                std::weak_ptr<connection::conversation> current(packet.socket->chat.load());
                if ( !current.lock() ) {
                    // Different hashes and probably no conversation
                    auto chat = std::make_shared<connection::conversation>(packet.socket);
                    if ( packet.socket->chat.compare_exchange_strong(current, chat) ) {
                        // Ok, certainly no ongoing conversation
                        logger("conversation", true);
                    } else {
                        logger("conversation", false);
                    }
                }
            }
        }
    }
}

