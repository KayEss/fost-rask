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
}


void rask::rehash_file(
    workers &w, subscriber &sub, const boost::filesystem::path &filename
) {
    ++p_files;
    std::size_t blocks{};
    const_file_block_hash_iterator end;
    for ( const_file_block_hash_iterator block(filename); block != end; ++block ) {
        ++blocks;
        if ( blocks > 1 )
            throw fostlib::exceptions::not_implemented("Files larger than 32KB are not yet supported");
    }
}
