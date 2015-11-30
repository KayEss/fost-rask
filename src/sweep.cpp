/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/configuration.hpp>
#include <rask/sweep.hpp>
#include <fost/crypto>

#include <boost/filesystem/fstream.hpp>

#include <vector>


using namespace fostlib;


struct rask::const_file_block_hash_iterator::impl {
    boost::filesystem::path filename;
    std::array<unsigned char, file_hash_block_size> block;
    std::size_t bytes, offset;
    boost::filesystem::ifstream file;

    impl(boost::filesystem::path path)
    : filename(std::move(path)), bytes(0), offset(0), file(filename) {
        read();
    }

    std::size_t read() {
        offset += bytes;
        bytes = file.read(reinterpret_cast<char *>(block.data()), block.size()).gcount();
        return bytes;
    }
};


rask::const_file_block_hash_iterator::const_file_block_hash_iterator() {
}

rask::const_file_block_hash_iterator::const_file_block_hash_iterator(
    const boost::filesystem::path &filename)
: pimpl(new impl(filename)) {
}

rask::const_file_block_hash_iterator::~const_file_block_hash_iterator() = default;


std::size_t rask::const_file_block_hash_iterator::offset() const {
    return pimpl->offset;
}
fostlib::const_memory_block rask::const_file_block_hash_iterator::data() const {
    const unsigned char * const begin = pimpl->block.data();
    return std::make_pair(begin, begin + pimpl->bytes);
}


rask::const_file_block_hash_iterator &
        rask::const_file_block_hash_iterator::operator ++ () {
    if ( pimpl->read() == 0u ) {
        pimpl.reset();
    };
    return *this;
}


bool rask::const_file_block_hash_iterator::operator == (
    const const_file_block_hash_iterator &r
) const {
    return pimpl == r.pimpl;
}


std::vector<unsigned char> rask::const_file_block_hash_iterator::operator * () const {
    digester hasher(sha256);
    if ( pimpl->bytes ) {
        hasher << data();
    }
    return hasher.digest();
}
