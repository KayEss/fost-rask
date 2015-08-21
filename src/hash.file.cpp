/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "file.hpp"
#include "hash.hpp"
#include "tree.hpp"
#include <rask/base32.hpp>
#include <rask/configuration.hpp>
#include <rask/subscriber.hpp>
#include <rask/sweep.hpp>
#include <rask/tenant.hpp>
#include <rask/workers.hpp>

#include <fost/counter>


namespace {
    fostlib::performance p_files(rask::c_fost_rask,
        "hash", "file", "ordered");
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
    const std::size_t size = blocks_total * 32;
    allocate_file(base_db_file, size);
    file.open(base_db_file, boost::iostreams::mapped_file_base::readwrite, size);
}


void rask::file::hashdb::operator () (
    std::size_t block, const std::vector<unsigned char> &hash
) {
    if ( std::memcmp(file.data() + block * 32, hash.data(), 32) != 0 ) {
        std::memcpy(file.data() + block * 32, hash.data(), 32);
    }
}

