/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/sweep.hpp>
#include <fost/crypto>
#include <boost/iostreams/device/mapped_file.hpp>
#include <vector>


using namespace fostlib;


struct rask::const_file_block_iterator::impl {
    boost::iostreams::mapped_file_source file;
    std::size_t size, offset;

    impl(const boost::filesystem::path &path)
    : size(boost::filesystem::file_size(path)), offset(0) {
        if ( size ) {
            file.open(path.string());
        }
    }
};


rask::const_file_block_iterator::const_file_block_iterator() {
}

rask::const_file_block_iterator::const_file_block_iterator(
    const boost::filesystem::path &filename)
: pimpl(new impl(filename)) {
}

rask::const_file_block_iterator::~const_file_block_iterator() {
}


void rask::const_file_block_iterator::operator ++ () {
    pimpl->offset += file_hash_block_size;
    if ( pimpl->offset >= pimpl->size ) {
        pimpl.reset();
    }
}


bool rask::const_file_block_iterator::operator == (const const_file_block_iterator &r) const {
    return pimpl == r.pimpl;
}


std::vector<unsigned char> rask::const_file_block_iterator::operator * () const {
    digester hasher(sha256);
    if ( pimpl->size ) {
        const unsigned char *begin = reinterpret_cast< const unsigned char * >(
            pimpl->file.data());
        const unsigned char *starts = begin + pimpl->offset;
        const unsigned char *ends = std::min(begin + pimpl->size, starts + file_hash_block_size);
        hasher << std::make_pair(starts, ends);
    }
    return hasher.digest();
}
