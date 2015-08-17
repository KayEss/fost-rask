/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include <rask/configuration.hpp>
#include <rask/sweep.hpp>
#include <rask/workers.hpp>

#include <fost/counter>


namespace {
    fostlib::performance p_files(rask::c_fost_rask,
        "hash", "file", "ordered");
    fostlib::performance p_blocks(rask::c_fost_rask,
        "hash", "file", "blocks");
}


void rask::rehash_file(
    workers &w, subscriber &sub, const boost::filesystem::path &filename,
    file_hash_callback callback
) {
    ++p_files;
    file::hashdb hash(1234);
    const_file_block_hash_iterator end;
    for ( const_file_block_hash_iterator block(filename); block != end; ++block ) {
        ++p_blocks;
        hash(*block);
    }
    callback(hash);
}


/*
    rask::file::level
*/


rask::file::level::level(std::size_t number)
: hasher(fostlib::sha256) {
}


/*
    rask:file::hashdb
*/


rask::file::hashdb::hashdb(std::size_t bytes)
: blocks_hashed(0),
    blocks_total((bytes + file_hash_block_size - 1) / file_hash_block_size)
{
}


void rask::file::hashdb::operator () (const std::vector<unsigned char> &hash) {
}

