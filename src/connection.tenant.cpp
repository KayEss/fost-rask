/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include "tree.hpp"
#include <rask/connection.hpp>
#include <rask/tenant.hpp>
#include <rask/workers.hpp>

#include <beanbag/beanbag>


rask::connection::out rask::tenant_packet(
    const fostlib::string &name, const fostlib::json &meta
) {
    connection::out packet(0x81);
    packet << name;
    const auto hash = fostlib::coerce<std::vector<unsigned char>>(
            fostlib::base64_string(
                fostlib::coerce<fostlib::ascii_string>(
                    fostlib::coerce<fostlib::string>(meta["hash"]["data"]))));
    packet << hash;
    return std::move(packet);
}


namespace {
    void send_tenant_content(
        std::shared_ptr<rask::tenant> tenant, std::shared_ptr<rask::connection> socket
    ) {
        for ( auto iter(tenant->inodes().begin()); iter != tenant->inodes().end(); ++iter ) {
            const fostlib::json inode(*iter);
            auto &filetype = inode["filetype"];
            if ( filetype == rask::tenant::directory_inode ) {
                fostlib::log::debug(rask::c_fost_rask)
                    ("", "sending create_directory")
                    ("inode", inode);
                socket->queue(
                    [tenant, &inode]() {
                        return create_directory_out(*tenant, rask::tick(inode["priority"]),
                            fostlib::coerce<fostlib::string>(inode["name"]));
                    });
            } else if ( filetype == rask::tenant::move_inode_out ) {
                fostlib::log::debug(rask::c_fost_rask)
                    ("", "sending move_out")
                    ("inode", inode);
                socket->queue(
                    [tenant, &inode]() {
                        return move_out_packet(*tenant, rask::tick(inode["priority"]),
                            fostlib::coerce<fostlib::string>(inode["name"]));
                    });
            } else {
                fostlib::log::error(rask::c_fost_rask)
                    ("", "Unkown inode type to send to peer")
                    ("inode", inode);
            }
        }
    }
}


void rask::tenant_packet(connection::in &packet) {
    auto logger(fostlib::log::info(c_fost_rask));
    logger
        ("", "Tenant packet")
        ("connection", packet.socket_id());
    auto name(packet.read<fostlib::string>());
    logger("name", name);
    auto hash(packet.read(32));
    auto hash64 = fostlib::coerce<fostlib::base64_string>(hash);
    logger("hash",  hash64.underlying().underlying().c_str());
    packet.socket->workers.high_latency.get_io_service().post(
        [socket = packet.socket, name = std::move(name), hash = std::move(hash)]() {
            if ( socket->identity ) {
                auto partner(peer::server(socket->identity));
                auto tenant(partner->tenants.add_if_not_found(name,
                    [](){ return std::make_shared<peer::tenant>(); }));
                std::array<unsigned char, 32> hash_array;
                std::copy(hash.begin(), hash.end(), hash_array.begin());
                tenant->hash = hash_array;
                if ( !c_subscriptions_db.value().isnull() ) {
                    // work out if we're subscribed....
                    beanbag::jsondb_ptr dbp(beanbag::database(c_subscriptions_db.value()));
                    fostlib::jsondb::local subscriptions(*dbp, "subscription");
                    if ( subscriptions.has_key(fostlib::jcursor("t1", "path")) ) {
                        // Ok, we really are subscribed to this tenant
                        send_tenant_content(known_tenant(name), socket);
                        return;
                    }
                }
                // We're not subscribed to this, so we just store the hash in our
                // tenants database so we can use it to calculate our server hash
            }
        });
}

