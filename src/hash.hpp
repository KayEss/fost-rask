/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/clock.hpp>

#include <beanbag/beanbag>

#include <fost/crypto>

#include <boost/iostreams/device/mapped_file.hpp>

#include <atomic>


namespace rask {


    class subscriber;
    class tenant;
    struct workers;


    /// The name hash
    using name_hash_type = fostlib::string;

    /// A single hash value that can be atomically updated
    using hash_value = std::atomic<std::array<unsigned char, 32>>;

    /// Return the hash for a name
    name_hash_type name_hash(const fostlib::string &);
    name_hash_type name_hash(const boost::filesystem::path &);

    /// Classes that implement the file hashing protocol
    namespace file {


        /// The block record in the file hash files
        struct block_hash {
            unsigned char hash[32];
        };

        /// An upper level in the hashing hierarchy.
        class level {
        public:
            /// Construct the level
            level(std::size_t number, const boost::filesystem::path &db);
        };

        /// The hash structure
        class hashdb : boost::noncopyable {
            boost::filesystem::path base_db_file;
            std::size_t blocks_hashed, blocks_total;
            std::vector<level> levels;

        public:
            /// Construct a hashdb for a file of the given size
            hashdb(std::size_t bytes, boost::filesystem::path dbfile);

            /// Add the next hash value to the database
            void operator () (const std::vector<unsigned char> &h);

            /// Fetch the single hash value for the file
            std::vector<unsigned char> operator () ();
        };


    }

    /// Callback for the file (re-)hasher
    using file_hash_callback = std::function<void(const file::hashdb &)>;
    /// Re-hash the pointed to file
    void rehash_file(workers &w, subscriber &,
        const boost::filesystem::path &, const fostlib::json &inode,
        file_hash_callback);

    /// Re-hash starting at specified database
    void rehash_inodes(workers&, beanbag::jsondb_ptr);
    void rehash_inodes(workers&, const fostlib::json &dbconfig);

    /// Re-hash starting at the tenants level
    void rehash_tenants(beanbag::jsondb_ptr);


    /// Convert a name hash to a file path with directory structure. No
    /// extension is added.
    name_hash_type name_hash_path(const name_hash_type &);


}

