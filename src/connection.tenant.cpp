/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include "tree.hpp"
#include <rask/connection.hpp>
#include <rask/subscriber.hpp>
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
        std::shared_ptr<rask::tenant> tenant,
        std::shared_ptr<rask::connection> socket,
        std::size_t layer, const rask::name_hash_type &prefix
    ) {
        auto dbp = tenant->subscription->inodes().layer_dbp(layer, prefix);
        fostlib::jsondb::local db(*dbp);
        if ( rask::partitioned(db) ) {
            throw fostlib::exceptions::not_implemented(
                "Sending a partitioned part of the tenant tree");
        } else {
            auto inodes = db["inodes"];
            for ( auto iter(inodes.begin()); iter != inodes.end(); ++iter ) {
                const fostlib::json inode(*iter);
                auto &filetype = inode["filetype"];
                if ( filetype == rask::tenant::directory_inode ) {
                    fostlib::log::debug(rask::c_fost_rask)
                        ("", "sending create_directory")
                        ("inode", inode);
                    socket->queue(
                        [tenant, inode]() {
                            return create_directory_out(*tenant, rask::tick(inode["priority"]),
                                fostlib::coerce<fostlib::string>(inode["name"]));
                        });
                } else if ( filetype == rask::tenant::move_inode_out ) {
                    fostlib::log::debug(rask::c_fost_rask)
                        ("", "sending move_out")
                        ("inode", inode);
                    socket->queue(
                        [tenant, inode]() {
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
    if ( packet.socket->identity ) {
        packet.socket->workers.high_latency.get_io_service().post(
            [socket = packet.socket, name = std::move(name), hash = std::move(hash)]() {
                auto tenant = known_tenant(socket->workers, name);
                if ( tenant->subscription ) {
                    send_tenant_content(tenant, socket, 0u, name_hash_type());
                } else {
                    // We're not subscribed to this, so we just store the hash in our
                    // tenants database so we can use it to calculate our server hash
                    throw fostlib::exceptions::not_implemented(
                        "Receiving a tenant packet where the tenant isn't subscribed to");
                }
            });
    }
}

