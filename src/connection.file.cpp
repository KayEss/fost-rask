/*
    Copyright 2015-2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
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

#include <boost/iostreams/device/mapped_file.hpp>


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
    return packet;
}


namespace {
    void create_and_watch_parent(
        rask::workers &w,
        std::shared_ptr<rask::tenant> tenant,
        const boost::filesystem::path &pathname
    ) {
        const auto parent = pathname.parent_path();
        if ( not boost::filesystem::exists(parent) ) {
            create_and_watch_parent(w, tenant, parent);
            boost::filesystem::create_directory(parent);
        }
        w.notify.watch(tenant, parent);
    }
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
    fostlib::json hash64;
    if ( !packet.empty() ) {
        size = packet.read<uint64_t>() & 0x7FFF'FFFF'FFFF'FFFF;
        logger("size", "bytes", size);
        if ( !packet.empty() ) {
            auto hash = packet.read(32);
            hash64 = fostlib::coerce<fostlib::json>(
                fostlib::coerce<fostlib::base64_string>(hash));
            logger("hash", hash64);
        }
    }
    if ( tenant->subscription ) {
        packet.socket->workers.files.get_io_service().post(
            [tenant, name = std::move(name), priority, size, hash64,
                socket = packet.socket
            ]() {
                static const fostlib::jcursor remote("remote");
                static const fostlib::jcursor hash_inode("hash", "inode");
                auto save_remote_hash =
                    [priority, size, hash64](auto j) {
                        if ( j.has_key(remote) ) remote.del_key(j);
                        if ( not j.has_key(hash_inode) ) {
                            fostlib::insert(j, "remote", "priority", priority);
                            fostlib::insert(j, "remote", "size", size);
                            fostlib::insert(j, "remote", "hash", hash64);
                        }
                        return j;
                    };
                tenant->subscription->file(name)
                    .compare_priority(priority)
                    .record_priority(fostlib::null)
                    .enrich_update(save_remote_hash)
                    .enrich_otherwise(save_remote_hash)
                    .post_commit(
                        [size, socket, hash64, tenant](auto&c) {
                            if ( !size.isnull() ) {
                                create_and_watch_parent(socket->workers, tenant, c.location);
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
    return packet;
}


namespace {

    class sending {
    public:
        /// Create the instance and track the first recipient
        sending(
            std::shared_ptr<rask::connection> socket,
            std::shared_ptr<rask::tenant> tenant,
            rask::tick priority,
            fostlib::string name,
            boost::filesystem::path loc
        ) : tenant(tenant), priority(priority), name(std::move(name)),
            file(std::move(loc)), position(std::make_pair(file.begin(), file.end()))
        {
        }

        std::shared_ptr<rask::tenant> tenant;
        const rask::tick priority;
        const fostlib::string name;
        const rask::file::data file;
        std::pair<rask::file::const_block_iterator,
            rask::file::const_block_iterator> position;

        /// Start sending a file to the recipient
        static void start(
            std::shared_ptr<rask::connection> socket,
            std::shared_ptr<rask::tenant> tenant, rask::tick priority,
            fostlib::string name, boost::filesystem::path location
        ) {
            auto sender = std::make_shared<sending>(socket, tenant,
                priority, std::move(name), std::move(location));
            sender->queue(sender, socket);
        }

        void queue(std::shared_ptr<sending> self,
            std::shared_ptr<rask::connection> socket);
    };
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
                    [socket, tenant, filename = std::move(filename), location](
                        const fostlib::json &inode
                    ) {
                        if ( inode.has_key("priority") ) {
                            try {
                                sending::start(socket, tenant, tick(inode["priority"]),
                                    fostlib::coerce<fostlib::string>(inode["name"]), location);
                            } catch ( std::exception &e ) {
                                fostlib::log::warning(c_fost_rask)
                                    ("", "Tried to send a file that no longer exists -- setting as deleted")
                                    ("function", "name", __FUNCTION__)
                                    ("inode", inode)
                                    ("exception", e.what());
                                tenant->subscription->move_out(filename)
                                    .broadcast(rask::move_out_packet)
                                    .execute();
                            }
                        }
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
        try {
            ++p_file_data_block_written;
            rask::connection::out packet(0x9f);
            auto data = *block;
            packet << priority << tenant.name() << name <<
                (fostlib::coerce<int64_t>(data.first));
            fostlib::digester hash(fostlib::sha256);
            hash << data.second;
            packet << hash.digest();
            packet.size_sequence(data.second) << data.second;
            return packet;
        } catch ( fostlib::exceptions::exception &e ) {
            fostlib::insert(e.data(), "tenant", tenant.name());
            fostlib::insert(e.data(), "location", location);
            throw;
        }
    }
    void sending::queue(
        std::shared_ptr<sending> self,
        std::shared_ptr<rask::connection> socket
    ) {
        if ( position.first != position.second ) {
            socket->queue(
                [this, self, socket]() {
                    auto packet =
                        send_file_block(*tenant, priority, name,
                            file.location(), position.first);
                    ++position.first;
                    queue(self, socket);
                    return packet;
                });
        }
    }
    void save_file_data(
        rask::workers &w,
        rask::subscriber &sub, fostlib::string filename,
        rask::tick priority, std::size_t offset, std::vector<unsigned char> data
    ) {
        auto logger(fostlib::log::debug(rask::c_fost_rask));
        logger
            ("", __FUNCTION__)
            ("tenant", sub.tenant.name())
            ("filename", filename)
            ("offset", offset)
            ("priority", priority);
        sub.file(filename)
            .compare_priority(priority)
            .if_predicate(
                [&w, &sub, offset, data = std::move(data)]
                    (auto &database, const auto &dbpath, auto &result)
                {
                    auto logger(fostlib::log::debug(rask::c_fost_rask));
                    logger("", "save_file_data -- Write data")
                        ("location", result.location)
                        ("offset", offset)
                        ("bytes", data.size());
                    const auto alignment =
                        boost::iostreams::mapped_file_sink::alignment();
                    logger("alignment", alignment);
                    if ( offset % alignment != 0 ) {
                        logger("action", "misaligned");
                    } else {
                        boost::iostreams::mapped_file_sink mmap;
                        mmap.open(result.location, data.size(), offset);
                        if ( mmap.is_open() && mmap.data() ) {
                            if ( std::memcmp(mmap.data(), data.data(), data.size()) != 0 ) {
                                logger("action", "write");
                                std::memcpy(mmap.data(), data.data(), data.size());
                            } else {
                                logger("action", "already-written");
                                rask::rehash_file(w, sub, result.location, database[dbpath], [](){});
                            }
                        } else {
                            logger("action", "mmap-failure");
                        }
                    }
                    return database[dbpath];
                })
            .execute();
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
    logger("size", size);
    /// Read the data, hash it and then make sure the hash is right
    auto data(packet.read(size));
    fostlib::digester hasher(fostlib::sha256);
    hasher << data;
    auto data_hash = hasher.digest();
    logger("data", "hash",
        fostlib::coerce<fostlib::base64_string>(data_hash));
    if ( data_hash != hash ) {
        logger("action", "throw");
        fostlib::exceptions::not_implemented e(__FUNCTION__,
            "Calculated data hash does not match sent data hash");
        fostlib::insert(e.data(), "tenant", tenant->name());
        fostlib::insert(e.data(), "filename", filename);
        fostlib::insert(e.data(), "offset", offset);
        fostlib::insert(e.data(), "size", size);
        fostlib::insert(e.data(), "hash",
            fostlib::coerce<fostlib::base64_string>(hash));
        fostlib::insert(e.data(), "data", "size", data.size());
        fostlib::insert(e.data(), "data", "hash",
            fostlib::coerce<fostlib::base64_string>(data_hash));
        throw e;
    } else if ( tenant->subscription ) {
        logger("action", "check");
        packet.socket->workers.files.get_io_service().post(
            [&w = packet.socket->workers, tenant, name = std::move(filename),
                priority, offset, data = std::move(data)
            ]() {
                save_file_data(w, *tenant->subscription, name, priority, offset, data);
            });
    } else {
        logger("action", "drop");
    }
}

