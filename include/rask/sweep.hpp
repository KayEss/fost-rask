/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/file>
#include <vector>


namespace rask {


    /// Use a 32KB block size for hashing a file
    const std::size_t file_hash_block_size = 32 * 1024;

    /// Iterator that can be used to go through a file block by block
    class const_file_block_iterator {
        /// The current block data
        std::vector<unsigned char> block_data;
    public:
        /// An end of file iterator
        const_file_block_iterator();

    };


}
