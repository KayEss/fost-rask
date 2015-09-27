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
    fostlib::performance p_file_data_block_received(
        rask::c_fost_rask, "packets", "file_data_block", "received");

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
        f5::tsmap<std::shared_ptr<rask::connection>,
            std::unique_ptr<
                std::pair<
                    rask::file::const_block_iterator,
                    rask::file::const_block_iterator>>> recipients;
        /// Create the instance and track the first recipient
        sending(
            std::shared_ptr<rask::connection> socket,
            std::shared_ptr<rask::tenant> tenant,
            rask::tick priority,
            fostlib::string name,
            boost::filesystem::path loc
        ) : tenant(tenant), priority(priority), name(std::move(name)),
            location(std::move(loc))
        {
            recipients.add_if_not_found(socket,
                [this]() {
                    return std::make_unique<
                            std::pair<
                                rask::file::const_block_iterator,
                                rask::file::const_block_iterator>
                        >(location.begin(), location.end());
                });
            queue(socket);
        }

    public:
        std::shared_ptr<rask::tenant> tenant;
        const rask::tick priority;
        const fostlib::string name;
        const rask::file::data location;

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
                    s.recipients.add_if_not_found(socket,
                        [&s]() {
                            /// We have added a new recipient for the file
                            fostlib::log::debug(rask::c_fost_rask)
                                ("", "Already sending file -- recipient added")
                                ("location", s.location.location());
                            return std::make_unique<
                                    std::pair<
                                        rask::file::const_block_iterator,
                                        rask::file::const_block_iterator>
                                >(s.location.begin(), s.location.end());
                        });
                    s.queue(socket);
                });
        }

        void queue(std::shared_ptr<rask::connection> socket);
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


namespace {
    rask::connection::out send_file_block(
        rask::tenant &tenant, const rask::tick &priority,
        const fostlib::string &name, const boost::filesystem::path &location,
        const rask::file::const_block_iterator &block
    ) {
        ++p_file_data_block_written;
        rask::connection::out packet(0x9f);
        packet << priority << tenant.name() << name <<
            (fostlib::coerce<int64_t>(block.offset()));
        fostlib::digester hash(fostlib::sha256);
        hash << *block;
        packet << hash.digest();
        packet.size_sequence(*block) << *block;
        return std::move(packet);
    }
    void sending::queue(std::shared_ptr<rask::connection> socket) {
        auto position = recipients.find(socket);
        if ( position ) {
            socket->queue(
                [this, socket, position]() {
                    auto packet = send_file_block(*tenant, priority, name,
                        location.location(), position->first);
                    if ( ++(position->first) == position->second ) {
                        g_sending.remove(location.location());
                    } else {
                        queue(socket);
                    }
                    return std::move(packet);
                });
        }
    }
}


void rask::file_data_block(connection::in &packet) {
    ++p_file_data_block_received;
    auto logger(fostlib::log::debug(c_fost_rask));
    logger
        ("", "File data block packet")
        ("connection", "id", packet.socket_id());
    auto priority(packet.read<tick>());
    logger("priority", priority);
    auto tenant(
        known_tenant(packet.socket->workers, packet.read<fostlib::string>()));
    logger("tenant", tenant->name());
    auto filename(packet.read<fostlib::string>());
    logger("filename", filename);
    auto offset(packet.read<int64_t>());
    logger("offset", offset);
    auto hash(packet.read(32));
    logger("hash", fostlib::coerce<fostlib::base64_string>(hash));
    auto size(packet.size_control());
    logger("data", "size", size);
    auto data(packet.read(size));
    fostlib::digester hasher(fostlib::sha256);
    hasher << data;
    auto data_hash = hasher.digest();
    logger("data", "hash",
        fostlib::coerce<fostlib::base64_string>(data_hash));
    if ( data_hash != hash ) {
        throw fostlib::exceptions::not_implemented(
            "Calculated data hash does not match sent data hash");
    }
}

