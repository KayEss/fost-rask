/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include "tree.hpp"
#include <rask/base32.hpp>
#include <rask/configuration.hpp>
#include <rask/subscriber.hpp>
#include <rask/sweep.hpp>
#include <rask/tenant.hpp>
#include <rask/workers.hpp>

#include <fost/counter>

#include <fcntl.h>


namespace {
    fostlib::performance p_files(rask::c_fost_rask,
        "hash", "file", "ordered");
    fostlib::performance p_blocks(rask::c_fost_rask,
        "hash", "file", "blocks");

    template<typename F> inline
    int syscall(F f) {
        int result{};
        do {
            result = f();
        } while ( result == -1 && errno == EINTR );
        return result;
    }

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
            db(blocks, *block);
        }
        return blocks;
    }
}


void rask::rehash_file(
    workers &w, subscriber &sub, const boost::filesystem::path &filename,
    const fostlib::json &inode, file_hash_callback callback
) {
    ++p_files;
    auto tdbpath = sub.inodes().dbpath(
        fostlib::coerce<boost::filesystem::path>(
            name_hash_path(inode["hash"]["name"].get<fostlib::string>().value())));
    auto current = filename;
    for ( uint8_t level{}; true;  ++level ) {
        auto dbpath = tdbpath;
        dbpath += "-" +
            fostlib::coerce<rask::base32_string>(level).underlying().underlying()
                + ".hashes";
        file::hashdb hash(boost::filesystem::file_size(current), dbpath);
        if ( hash_layer(current, hash) <= 1 ) {
            callback(hash);
            fostlib::log::info(c_fost_rask)
                ("", "Hashing of file completed")
                ("tenant", sub.tenant.name())
                ("filename", filename)
                ("levels", level + 1);
            return;
        }
        current = dbpath;
    }
}


/*
    rask:file::hashdb
*/


rask::file::hashdb::hashdb(std::size_t bytes, boost::filesystem::path dbf)
: base_db_file(std::move(dbf)),
    blocks_total(std::max(1ul, (bytes + file_hash_block_size - 1) / file_hash_block_size))
{
    boost::filesystem::create_directories(base_db_file.parent_path());
    // Resize the file, probably overkill on the complexity front
    int opened = syscall([&]() {
            const int flags = O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW;
            // user read/write, group read/write, world read
            const int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
            return open(base_db_file.c_str(), flags, mode);
        });
    if ( opened >= 0 ) {
        const int alloc = syscall([&]() {
                const int mode = 0u;
                const off_t offset = 0u;
                const auto bytes = blocks_total * sizeof(file::block_hash);
                return fallocate(opened, mode, offset, bytes);
            });
        const auto alloc_err = errno;
        syscall([&]() { return close(opened); });
        if ( alloc == -1 ) {
            std::error_code error(alloc_err, std::system_category());
            fostlib::log::error(c_fost_rask,"fallocate",  error.message().c_str());
            throw fostlib::exceptions::not_implemented(
                "Could not change allocate size of the hash database file",
                error.message().c_str());
        }
    } else {
        std::error_code error(errno, std::system_category());
        fostlib::log::error(c_fost_rask, "open", error.message().c_str());
        throw fostlib::exceptions::not_implemented(
            "Bad file descriptor for hash database file", error.message().c_str());
    }
    file.open(base_db_file, boost::iostreams::mapped_file_base::readwrite,
        blocks_total * sizeof(file::block_hash));
}


void rask::file::hashdb::operator () (
    std::size_t block, const std::vector<unsigned char> &hash
) {
}

