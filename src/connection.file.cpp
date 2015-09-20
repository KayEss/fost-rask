/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "file.hpp"
#include "subscriber.hpp"
#include <rask/connection.hpp>
#include <rask/tenant.hpp>

#include <fost/counter>


namespace {
    fostlib::performance p_file_exists_received(
        rask::c_fost_rask, "packets", "file_exists", "received");
    fostlib::performance p_file_exists_written(
        rask::c_fost_rask, "packets", "file_exists", "written");

    const fostlib::jcursor jc_priority("priority");
}


rask::connection::out rask::file_exists_out(
    tenant &t, const tick &p, const fostlib::string &n, const fostlib::json &inode
) {
    ++p_file_exists_written;
    connection::out packet(0x90);
    packet << p << t.name() << n;
    if ( !inode["stat"].isnull() ) {
        /// TODO: Make sure that there's a test that this will throw if we
        /// overflow 63 bits of file size, although in pracitice we can
        /// reduce the maximum file size we accept in the JSON further
        /// as it'll likely be some years before we see files larger than
        /// somewhere around the mid-40s of bits (the TB range).
        packet << fostlib::coerce<uint64_t>(inode["stat"]["size"]["bytes"]);
        if ( !inode["hash"].isnull() && !inode["hash"]["inode"].isnull() ) {
            auto hash64 = fostlib::base64_string(
                fostlib::coerce<fostlib::string>(inode["hash"]["inode"]).c_str());
            auto hash = fostlib::coerce<std::vector<unsigned char>>(hash64);
            packet << hash;
        }
    }
    return std::move(packet);
}


void rask::file_exists(rask::connection::in &packet) {
    ++p_file_exists_received;
    auto logger(fostlib::log::info(c_fost_rask));
    logger("", "File exists");
    auto priority(packet.read<tick>());
    logger("priority", priority);
    auto tenant(
        known_tenant(packet.socket->workers, packet.read<fostlib::string>()));
    auto name(packet.read<fostlib::string>());
    logger
        ("tenant", tenant->name())
        ("name", name);
    fostlib::nullable<uint64_t> size;
    fostlib::base64_string hash64;
    if ( !packet.empty() ) {
        size = packet.read<uint64_t>() & 0x7FFF'FFFF'FFFF'FFFF;
        logger("size", "bytes", size);
        if ( !packet.empty() ) {
            auto hash = packet.read(32);
            hash64 = fostlib::coerce<fostlib::base64_string>(hash);
            logger("hash", hash64);
        }
    }
    if ( tenant->subscription ) {
        packet.socket->workers.files.get_io_service().post(
            [tenant, name = std::move(name), priority, size, hash64]() {
                tenant->subscription->file(name)
                    .compare_priority(priority)
                    .record_priority(fostlib::null)
                    .enrich_update(
                        [priority, size, hash64](auto j) {
                            fostlib::insert(j, "remote", "priority", priority);
                            fostlib::insert(j, "remote", "size", size);
                            fostlib::insert(j, "remote", "hash", hash64);
                            return j;
                        })
                    .post_commit(
                        [size](const auto&c) {
                            if ( !size.isnull() ) {
                                allocate_file(c.location,
                                    fostlib::coerce<std::size_t>(size.value()));
                            }
                        })
                    .execute();
            });
    }
}

