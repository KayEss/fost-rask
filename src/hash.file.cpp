/*
    Copyright 2015-2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "file.hpp"
#include "hash.hpp"
#include "subscriber.hpp"
#include "tree.hpp"
#include <rask/base32.hpp>
#include <rask/configuration.hpp>
#include <rask/sweep.hpp>
#include <rask/tenant.hpp>
#include <rask/workers.hpp>

#include <f5/threading/set.hpp>
#include <fost/counter>


namespace {
    fostlib::performance p_files(rask::c_fost_rask,
        "hash", "file", "ordered");
    fostlib::performance p_skipped_nc(rask::c_fost_rask,
        "hash", "file", "skipped", "no-change");
    fostlib::performance p_skipped_hashing(rask::c_fost_rask,
        "hash", "file", "skipped", "currently-hashing");
    fostlib::performance p_skipped_gone(rask::c_fost_rask,
        "hash", "file", "skipped", "gone");
    fostlib::performance p_skipped_other(rask::c_fost_rask,
        "hash", "file", "skipped", "other");
    fostlib::performance p_restarted(rask::c_fost_rask,
        "hash", "file", "re-started");
    fostlib::performance p_started(rask::c_fost_rask,
        "hash", "file", "started");
    fostlib::performance p_completed(rask::c_fost_rask,
        "hash", "file", "completed");
    fostlib::performance p_blocks(rask::c_fost_rask,
        "hash", "file", "blocks");

    std::size_t hash_layer(
        const boost::filesystem::path &filename,
        rask::file::hashdb &db
    ) {
        fostlib::log::debug(rask::c_fost_rask)
            ("", "Hashing one layer for a file")
            ("filename", filename);
        std::size_t blocks{};
        using biter = rask::const_file_block_hash_iterator;
        const biter end;
        for ( biter block(filename); block != end; ++block, ++blocks ) {
            ++p_blocks;
            if ( not db(blocks, *block) ) {
                /// We've run off the end of the block database file which
                /// means that the file size has changed under us. Abandon
                /// this hashing run as we're going to be starting again
                /// from the beginning anyway
                return 0;
            }
        }
        return blocks;
    }

    template<typename C>
    void do_hashing(
        rask::subscriber &sub,
        const boost::filesystem::path &filename,
        const fostlib::json &inode, C callback
    ) {
        ++p_started;
        auto tdbpath = sub.inodes().dbpath(
            fostlib::coerce<boost::filesystem::path>(
                rask::name_hash_path(inode["hash"]["name"].get<fostlib::string>().value())));
        auto current = filename;
        for ( uint8_t level{}; true;  ++level ) {
            auto dbpath = tdbpath;
            dbpath += "-" +
                fostlib::coerce<rask::base32_string>(level).underlying().underlying()
                    + ".hashes";
            rask::file::hashdb hash(boost::filesystem::file_size(current), dbpath, level);
            if ( hash_layer(current, hash) <= 1 ) {
                callback(hash);
                ++p_completed;
                return;
            }
            current = dbpath;
        }
    }

    f5::tsset<boost::filesystem::path> g_hashing;
}


void rask::rehash_file(
    workers &w, subscriber &sub, const boost::filesystem::path &filename,
    const fostlib::json &inode, file_hash_callback callback
) {
    if ( !boost::filesystem::exists(filename) ) {
        ++p_skipped_gone;
        if ( inode["priority"].isnull() ) {
            fostlib::log::debug(c_fost_rask)
                ("", "Not hashing as the file hasn't been recieved yet")
                ("tenant", sub.tenant.name())
                ("inode", inode);
        } else {
            // TODO: Adding a move-out inode to the database is probably
            // the wrong thing to do in many circumstances
            fostlib::log::warning(c_fost_rask)
                ("", "Not hashing as the file is gone from the file system -- "
                    "this could well be the wrong thing to do")
                ("tenant", sub.tenant.name())
                ("inode", inode);
        }
        callback();
        return;
    } else if ( !boost::filesystem::is_regular_file(filename) ) {
        ++p_skipped_other;
        callback();
        fostlib::log::debug(c_fost_rask)
            ("", "Not hashing as the file is not a regular file")
            ("tenant", sub.tenant.name())
            ("inode", inode);
        return;
    }
    auto before_status = file_stat(filename);
    if ( !inode["stat"].isnull() && stat(inode["stat"]) == before_status ) {
        ++p_skipped_nc;
        callback();
        fostlib::log::debug(c_fost_rask)
            ("", "Not hashing as old and new file stats match")
            ("tenant", sub.tenant.name())
            ("stat", "now", before_status)
            ("inode", inode);
        return;
    }
    if ( !g_hashing.insert_if_not_found(filename) ) {
        ++p_skipped_hashing;
        fostlib::log::debug(c_fost_rask)
            ("", "Not hashing as the file is already being hashed")
            ("tenant", sub.tenant.name())
            ("inode", inode);
        callback();
        return;
    }
    fostlib::log::debug(c_fost_rask)
        ("", "Going to start hashing a file as the stats don't match")
        ("stat", "now", before_status)
        ("stat", "inode", inode["stat"].isnull() ? fostlib::json() :
            fostlib::coerce<fostlib::json>(stat(inode["stat"])))
        ("inode", inode);
    ++p_files;
    w.hashes.get_io_service().post(
        [&w, &sub, filename, inode, callback, before_status]() {
            do_hashing(sub, filename, inode,
                [&w, &sub, filename, inode, callback, before_status](auto &hash) {
                    auto after_status = file_stat(filename);
                    if ( before_status == after_status ) {
                        auto hash_value = fostlib::coerce<fostlib::string>(
                            fostlib::coerce<fostlib::base64_string>(hash(0)));
                        fostlib::log::debug(c_fost_rask)
                            ("", "Recording hash value for file")
                            ("tenant", sub.tenant.name())
                            ("filename", filename)
                            ("hash", hash_value);
                        sub.file(filename)
                            .predicate([hash_value](const fostlib::json &inode) {
                                if ( inode["remote"].isnull() )
                                    return inode["hash"].isnull() ||
                                        inode["hash"]["inode"] != fostlib::json(hash_value);
                                else
                                    return inode["remote"]["hash"] == fostlib::json(hash_value);
                            })
                            .record_priority(
                                [](const fostlib::json &oldnode) {
                                    if ( oldnode["remote"].isnull() )
                                        return tick::next();
                                    else
                                        return tick(oldnode["remote"]["priority"]);
                                })
                            .hash([hash_value](const tick &, const fostlib::json &) {
                                return fostlib::json(hash_value);
                            })
                            .enrich_update([after_status](fostlib::json j) {
                                fostlib::insert(j, "stat", after_status);
                                return j;
                            })
                            .broadcast(rask::file_exists_out)
                            .post_update(
                                [level = hash.level() + 1](
                                    const auto &c
                                ) {
                                    fostlib::log::info(c_fost_rask)
                                        ("", "Recorded stable file hash")
                                        ("tenant", c.subscription.tenant.name())
                                        ("filename", c.location)
                                        ("inode", "old", c.old)
                                        ("inode", "new", c.inode)
                                        ("levels", level);
                                })
                            .enrich_otherwise(
                                [after_status](fostlib::json node) {
                                    static fostlib::jcursor stat("stat");
                                    const auto status =
                                        fostlib::coerce<fostlib::json>(after_status);
                                    if ( node.has_key(stat) )
                                        stat.replace(node, status);
                                    else
                                        stat.insert(node, status);
                                    return node;
                                })
                            .post_otherwise(
                                [hash_value](const auto &c) {
                                    fostlib::log::info(c_fost_rask)
                                        ("", "Stable hash is no different to the existing one")
                                        ("tenant", c.subscription.tenant.name())
                                        ("filename", c.location)
                                        ("hash", hash_value);
                                })
                            .execute();
                        g_hashing.remove(filename);
                        callback();
                    } else {
                        ++p_restarted;
                        fostlib::log::info(c_fost_rask)
                            ("", "File stat changed during hashing, going again")
                            ("tenant", sub.tenant.name())
                            ("filename", filename)
                            ("inode", "old", inode)
                            ("levels", hash.level() + 1)
                            ("stat", "before", before_status)
                            ("stat", "now", after_status);
                        g_hashing.remove(filename);
                        rehash_file(w, sub, filename, inode, callback);
                    }
                });
        });
}


/*
    rask:file::hashdb
*/


rask::file::hashdb::hashdb(
    std::size_t bytes, boost::filesystem::path dbf, std::size_t level
) : db_file(std::move(dbf)),
    blocks_total(std::max(1ul, (bytes + file_hash_block_size - 1) / file_hash_block_size)),
    m_level(level)
{
    const std::size_t size = blocks_total * 32;
    allocate_file(db_file, size);
    file.open(db_file, boost::iostreams::mapped_file_base::readwrite, size);
}


bool rask::file::hashdb::operator () (
    std::size_t block, const std::vector<unsigned char> &hash
) {
    if ( block >= blocks_total ) {
        // TODO: This ought to be a bit smarter about what it's going to do
        fostlib::log::warning(c_fost_rask)
            ("", "Trying to write a hash block beyond the end of the database."
                "This implies that the file has grown since the hash database was created")
            ("total-blocks", blocks_total)
            ("block", block);
        return false;
    }
    if ( std::memcmp(file.data() + block * 32, hash.data(), 32) != 0 ) {
        std::memcpy(file.data() + block * 32, hash.data(), 32);
    }
    return true;
}


std::vector<unsigned char> rask::file::hashdb::operator () (std::size_t b) const {
    if ( b >= blocks_total )
        throw fostlib::exceptions::out_of_range<std::size_t>(
            "Block requested beyond the end of the hash database",
            b, 0, blocks_total);
    return std::vector<unsigned char>(file.data() + b * 32, file.data() + (b+1) * 32);
}

