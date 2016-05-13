/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "peer.hpp"
#include "subscriber.hpp"
#include "tree.hpp"
#include <rask/base32.hpp>
#include <rask/connection.hpp>
#include <rask/tenant.hpp>
#include <rask/workers.hpp>

#include <beanbag/beanbag>

#include <fost/counter>


namespace {
    fostlib::performance p_tenant_packet_received(
        rask::c_fost_rask, "packets", "tenant_packet", "received");
    fostlib::performance p_tenant_hash_packet_received(
        rask::c_fost_rask, "packets", "tenant_hash_packet", "received");
}


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


rask::connection::out rask::tenant_packet(
    rask::tenant &tenant, std::size_t layer, const rask::name_hash_type &prefix,
    const fostlib::json &data
) {
    connection::out packet(0x82);
    packet << tenant.name();
    packet << prefix.substr(0, layer);
    if ( partitioned(data) ) {
        for ( auto iter(data["inodes"].begin()); iter != data["inodes"].end(); ++iter ) {
            static const fostlib::jcursor hashloc("hash", "inode");
            if ( iter->has_key(hashloc) ) {
                auto key = fostlib::coerce<fostlib::string>(iter.key());
                if ( key.length() != 1 ) {
                    throw fostlib::exceptions::not_implemented(
                        "Error handling where the inode hash suffix is corrupt");
                }
                packet << from_base32_ascii_digit(key[0]);
                auto hash64 = fostlib::base64_string(
                    fostlib::coerce<fostlib::string>((*iter)[hashloc]).c_str());
                auto hash = fostlib::coerce<std::vector<unsigned char>>(hash64);
                packet << hash;
            }
        }
    }
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
            socket->queue(
                [tenant, layer, prefix, data = db.data()]() {
                    return tenant_packet(*tenant, layer, prefix, data);
                });
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
                            return create_directory_out(*tenant,
                                rask::tick(inode["priority"]),
                                fostlib::coerce<fostlib::string>(inode["name"]),
                                inode);
                        });
                } else if ( filetype == rask::tenant::file_inode ) {
                    if ( not inode["priority"].isnull() ) {
                        fostlib::log::debug(rask::c_fost_rask)
                            ("", "sending file_exists")
                            ("inode", inode);
                        socket->queue(
                            [tenant, inode]() {
                                return file_exists_out(*tenant,
                                    rask::tick(inode["priority"]),
                                    fostlib::coerce<fostlib::string>(inode["name"]),
                                    inode);
                            });
                    } else {
                        fostlib::log::warning(rask::c_fost_rask)
                            ("", "Have file inode without priority")
                            ("inode", inode);
                    }
                } else if ( filetype == rask::tenant::move_inode_out ) {
                    fostlib::log::debug(rask::c_fost_rask)
                        ("", "sending move_out")
                        ("inode", inode);
                    socket->queue(
                        [tenant, inode]() {
                            return move_out_packet(*tenant,
                                rask::tick(inode["priority"]),
                                fostlib::coerce<fostlib::string>(inode["name"]),
                                inode);
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
    ++p_tenant_packet_received;
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
        packet.socket->workers.responses.get_io_service().post(
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


void rask::tenant_hash_packet(connection::in &packet) {
    ++p_tenant_hash_packet_received;
    auto logger(fostlib::log::info(c_fost_rask));
    logger
        ("", "Tenant hash packet")
        ("connection", packet.socket_id());
    auto name(packet.read<fostlib::string>());
    logger("name", name);
    auto prefix(packet.read<fostlib::string>());
    std::size_t layer(prefix.length());
    logger
        ("prefix", prefix)
        ("layer", layer);
    std::array<fostlib::base64_string, 32> hashes;
    while ( !packet.empty() ) {
        auto suffix = packet.read<uint8_t>() & 31;
        auto hash = packet.read(32);
        hashes[suffix] = fostlib::coerce<fostlib::base64_string>(hash);
        logger("hash", fostlib::string(1, to_base32_ascii_digit(suffix)), hashes[suffix]);
    }
    packet.socket->workers.responses.get_io_service().post(
        [
            socket = packet.socket, name = std::move(name), layer,
            prefix = std::move(prefix), hashes = std::move(hashes)
        ]() {
            auto tenant = known_tenant(socket->workers, name);
            if ( tenant->subscription ) {
                auto dbp = tenant->subscription->inodes().layer_dbp(layer, prefix);
                fostlib::jsondb::local db(*dbp);
                if ( partitioned(db) ) {
                    bool found_difference = false;
                    auto subs = db["inodes"];
                    for ( auto iter(subs.begin()); iter != subs.end(); ++iter ) {
                        auto key = fostlib::coerce<fostlib::string>(iter.key());
                        if ( key.length() != 1 ) {
                            throw fostlib::exceptions::not_implemented(
                                "Error handling where the key in a partition is not 1 character");
                        }
                        static const fostlib::jcursor hashloc("hash", "inode");
                        if ( (*iter).has_key(hashloc) && !(*iter)[hashloc].isnull() &&
                                hashes[from_base32_ascii_digit(key[0])].underlying().underlying().c_str() !=
                                    (*iter)[hashloc].get<fostlib::string>().value() ) {
                            auto next = prefix + key;
                            send_tenant_content(tenant, socket, layer + 1, next);
                            found_difference = true;
                        }
                    }
                    if ( not found_difference ) {
                        /// We've cross indexed our hashes with the ones we got
                        /// and they all came out the same :( We'll re-hash
                        /// just to see if the error is on our side
                        rehash_inodes(socket->workers, dbp);
                    }
                } else {
                    /// The remote server has a lot more files here than we do.
                    /// We can tell this because they've sent us a mid-level
                    /// tenant hash packet and we have a non-partitioned
                    /// database of actual file hashes.
                    socket->queue(
                        [tenant, layer, prefix = std::move(prefix), data = db.data()]() {
                            return tenant_packet(*tenant, layer, prefix, data);
                        });
                }
            }
        });
}

