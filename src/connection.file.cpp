/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "file.hpp"
#include "subscriber.hpp"
#include "tree.hpp"
#include <rask/connection.hpp>
#include <rask/tenant.hpp>
#include <rask/sweep.hpp>

#include <f5/threading/map.hpp>
#include <f5/threading/set.hpp>
#include <fost/counter>


namespace {
    fostlib::performance p_file_exists_received(
        rask::c_fost_rask, "packets", "file_exists", "received");
    fostlib::performance p_file_hash_no_priority_received(
        rask::c_fost_rask, "packets", "file_hash_no_priority", "received");

    fostlib::performance p_file_exists_written(
        rask::c_fost_rask, "packets", "file_exists", "written");
    fostlib::performance p_empty_file_hash_written(
        rask::c_fost_rask, "packets", "file_hash_no_priority", "written");
    fostlib::performance p_file_data_block_written(
        rask::c_fost_rask, "packets", "file_data_block", "written");
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
            [
                tenant, name = std::move(name), priority, size, hash64,
                socket = packet.socket
            ]() {
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
                        [size, socket, hash64 = fostlib::coerce<fostlib::json>(hash64)](auto&c) {
                            if ( !size.isnull() ) {
                                allocate_file(c.location,
                                    fostlib::coerce<std::size_t>(size.value()));
                            }
                            if ( socket->identity && hash64 != c.inode["hash"]["inode"] ) {
                                auto logger(fostlib::log::debug(c_fost_rask));
                                logger
                                    ("", "sendfile")
                                    ("connection", "id", socket->id)
                                    ("connection", "peer", socket->identity.load())
                                    ("tenant", c.subscription.tenant.name())
                                    ("location", c.location);
                                if ( c.inode.has_key("remote") ) {
                                    logger("receiving", true);
                                    socket->queue(
                                        [&tenant = c.subscription.tenant, inode = c.inode]() {
                                            /// TODO: This should really send the current
                                            /// hashes so that we don't redundantly keep
                                            /// sending the same file data over and over.
                                            /// Of course if we've just created this file then
                                            /// we do want to send this as we don't have
                                            /// any file data at all.
                                            return send_empty_file_hash(tenant, inode);
                                        });
                                } else
                                    logger("receiving", false);
                            }
                        })
                    .execute();
            });
    }
}


rask::connection::out rask::send_empty_file_hash(
    rask::tenant &tenant, const fostlib::json &inode
) {
    ++p_empty_file_hash_written;
    connection::out packet(0x83);
    packet << tenant.name();
    packet << fostlib::coerce<fostlib::string>(inode["name"]);
    return std::move(packet);
}


namespace {

    class sending {
        /// Globally track the files we're sending
        /// TODO: Track the sending instance as a weak_ptr here and then
        /// the shared_ptr goes in the closure for queueing packets
        static f5::tsmap<boost::filesystem::path, std::unique_ptr<sending>>
            g_sending;
        /// Locally track who we are sending to
        f5::tsset<std::shared_ptr<rask::connection>> recipients;
        /// Create the instance and track the first recipient
        sending(
            std::shared_ptr<rask::connection> socket,
            std::shared_ptr<rask::tenant> tenant,
            rask::tick priority,
            fostlib::string name,
            boost::filesystem::path loc
        ) : tenant(tenant), priority(priority), name(std::move(name)),
            location(std::move(loc)), position(location)
        {
            recipients.insert_if_not_found(socket);
            queue();
        }

    public:
        std::shared_ptr<rask::tenant> tenant;
        const rask::tick priority;
        const fostlib::string name;
        const boost::filesystem::path location;
        rask::const_file_block_hash_iterator position, end;

        /// Start sending a file to the recipient if we're not already doing so.
        /// If we are already sending the file then we're going to attach this
        /// recipient to the data as it goes out
        static void start(
            std::shared_ptr<rask::connection> socket,
            std::shared_ptr<rask::tenant> tenant, rask::tick priority,
            fostlib::string name, boost::filesystem::path location
        ) {
            g_sending.add_if_not_found(
                location,
                [socket, tenant, priority, name, location]() {
                    fostlib::log::info(rask::c_fost_rask)
                        ("", "Starting send of file")
                        ("location", location);
                    return new sending(socket, tenant, priority,
                        std::move(name), std::move(location));
                },
                [socket](auto &s) {
                    if ( s.recipients.insert_if_not_found(socket) ) {
                        s.queue();
                        /// We have added a new recipient for the file
                        fostlib::log::debug(rask::c_fost_rask)
                            ("", "Already sending file -- recipient added")
                            ("location", s.location);
                    } else {
                        fostlib::log::debug(rask::c_fost_rask)
                            ("", "Already sending file -- recipient already receiving")
                            ("location", s.location);
                    }
                });
        }

        void queue() {
            /// TODO: This needs to use tsmap with an iterator per receiver
            /// and remove_if to drop those that have been dropped
            recipients.for_each(
                [this](auto &socket) {
                    socket->queue(
                        [this]() {
                            auto packet = send_file_block(*tenant, priority, name,
                                location, position);
                            if ( ++position == end ) {
                                g_sending.remove(location);
                            } else {
                                queue();
                            }
                            return std::move(packet);
                        });
                });
        }
    };

    f5::tsmap<boost::filesystem::path, std::unique_ptr<sending>>
            sending::g_sending;
}


void rask::file_hash_without_priority(connection::in &packet) {
    ++p_file_hash_no_priority_received;
    auto logger(fostlib::log::debug(c_fost_rask));
    logger
        ("", "File hash packet without priority")
        ("connection", "id", packet.socket_id());
    auto tenant(
        known_tenant(packet.socket->workers, packet.read<fostlib::string>()));
    logger("tenant", tenant->name());
    auto filename(packet.read<fostlib::string>());
    logger("filename", filename);
    if ( tenant->subscription ) {
        logger("subscribed", true);
        packet.socket->workers.files.get_io_service().post(
            [socket = packet.socket, tenant, filename = std::move(filename)]() {
                auto location = tenant->subscription->local_path() /
                    fostlib::coerce<boost::filesystem::path>(filename);
                tenant->subscription->inodes().lookup(
                    name_hash(filename), location,
                    [socket, tenant, location](
                        const fostlib::json &inode
                    ) {
                        if ( inode.has_key("priority") )
                            sending::start(socket, tenant, tick(inode["priority"]),
                                fostlib::coerce<fostlib::string>(inode["name"]), location);
                    });
            });
    } else {
        logger("subscribed", false);
    }
}


rask::connection::out rask::send_file_block(
    rask::tenant &tenant, const tick &priority,
    const fostlib::string &name, const boost::filesystem::path &location,
    const const_file_block_hash_iterator &block
) {
    ++p_file_data_block_written;
    connection::out packet(0x9f);
    packet << priority << tenant.name() << name <<
        (fostlib::coerce<int64_t>(block.offset()) / file_hash_block_size);
    packet << block.data();
    return std::move(packet);
}

