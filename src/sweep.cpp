/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/sweep.hpp>
#include <vector>


struct rask::const_file_block_iterator::impl {
    std::vector<unsigned char> buffer;
};


rask::const_file_block_iterator::const_file_block_iterator() {
}

rask::const_file_block_iterator::const_file_block_iterator(
    const boost::filesystem::path &
) {
}

rask::const_file_block_iterator::~const_file_block_iterator() {
}


void rask::const_file_block_iterator::operator ++ () {
}


bool rask::const_file_block_iterator::operator == (const const_file_block_iterator &) const {
    return true;
}

