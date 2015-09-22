/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/file>
#include <fost/pointers>


namespace rask {


    /// Use a 32KB block size for hashing a file
    const std::size_t file_hash_block_size = 32 * 1024;

    /// Iterator that can be used to go through a file block by block
    class const_file_block_hash_iterator final {
        struct impl;
        std::unique_ptr<impl> pimpl;
    public:
        /// The start iterator for hashing a file
        const_file_block_hash_iterator(const boost::filesystem::path &);
        /// An end of file iterator
        const_file_block_hash_iterator();
        /// Destructor so we can use pimpl;
        ~const_file_block_hash_iterator();

        /// Return the offset from the start of the file of the beginning
        /// of the block
        std::size_t offset() const;
        /// Return the underlying data that we're at
        fostlib::const_memory_block data() const;

        /// Move to the next block
        void operator ++ ();
        /// Allow comparison
        bool operator == (const const_file_block_hash_iterator &) const;

        /// Return the hash for the current block
        std::vector<unsigned char> operator * () const;
    };


}
