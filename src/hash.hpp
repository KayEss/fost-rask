/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <beanbag/beanbag>

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

    /// Re-hash the pointed to file
    void rehash_file(workers &w, subscriber &,
        const boost::filesystem::path &);

    /// Re-hash starting at specified database
    void rehash_inodes(workers&, beanbag::jsondb_ptr);
    void rehash_inodes(workers&, const fostlib::json &dbconfig);

    /// Re-hash starting at the tenants level
    void rehash_tenants(beanbag::jsondb_ptr);


}

