/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/datetime>
#include <fost/pointers>

#include <boost/filesystem/path.hpp>


namespace rask {


    /// Ensures that the filename requested is large enough to store the
    /// requested number of bytes. The file is enlarged (padded with zero)
    /// or shrunk as needed.
    void allocate_file(const boost::filesystem::path &, std::size_t);

    /// File stat structure giving relevant types
    struct stat {
        stat(const stat &) = default;
        stat(int64_t, fostlib::timestamp);
        stat(const fostlib::json &);

        int64_t size;
        fostlib::timestamp modified;

        bool operator == (const stat &s) const {
            return size == s.size && modified == s.modified;
        }
    };

    /// Return the stat information about a file. This allows size and
    /// modified time to be returned from one syscall.
    rask::stat file_stat(const boost::filesystem::path &);


    /// Calculate a relative path
    fostlib::string relative_path(
        const fostlib::string &root, const boost::filesystem::path &location);


    namespace file {


        class const_block_iterator;


        /// A file that is undergoing synchronisation in either direction, or
        /// hashing. State is shared between different processes that need
        /// to be able to read or write the file.
        class data {
            friend class const_block_iterator;
            /// Underlying implementation of the file wrapper
            struct impl;
            std::shared_ptr<impl> pimpl;
        public:
            /// Wrap an existing file on the filesystem
            data(boost::filesystem::path location);

            /// Store the file location
            const fostlib::accessors<boost::filesystem::path> location;

            /// The start of the file data
            const_block_iterator begin() const;
            /// The end of the file data
            const_block_iterator end() const;
        };


        /// An iterator that is able to walk a data file (target) or a hash
        /// file (meta-data).
        class const_block_iterator {
            /// Stored in order to make sure that the underlying file is kept
            /// alive.
            std::shared_ptr<data::impl> keep_alive;

        public:
            /// Increment the iterator
            const_block_iterator operator ++ ();
            /// The offset of the current data block
            std::size_t offset() const;
            /// The data
            fostlib::const_memory_block operator *() const;
            /// Return true if the two iterators point to the same thing
            bool operator == (const const_block_iterator &) const;
        };


    }


}


namespace fostlib {


    /// Allow coercion of the file stats
    template<>
    struct coercer<json, rask::stat> {
        json coerce(const rask::stat &);
    };


    /// Allow coercion from JSON to rask::stat
    template<>
    struct coercer<rask::stat, json> {
        rask::stat coerce(const json &j) {
            return rask::stat(j);
        }
    };


}

