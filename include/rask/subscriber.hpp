/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/file>

#include <rask/connection.hpp>

#include <beanbag/beanbag>


namespace rask {


    class tenant;
    class tick;
    class tree;


    /// The type of a function used to build inode packets
    typedef std::function<
        connection::out(tenant &, const rask::tick &, const fostlib::string &)> packet_builder;
    /// The type of the function used to build a hash of a file inode
    typedef std::function<
        fostlib::nullable<fostlib::base64_string>(const rask::tick &)> hasher_function;


    /// The part of the tenant that is a subscriber
    class subscriber {
        /// The tenant that this subscription is part of
        rask::tenant &tenant;
        /// Used for internal caclulations
        const fostlib::string root;
    public:
        /// Constructs a subscriber by providing a local path and the tenant
        subscriber(workers &, rask::tenant &, fostlib::string local);

        /// The local filesystem path
        const fostlib::accessors<boost::filesystem::path> local_path;

        /// The tenant beanbag with file system information
        beanbag::jsondb_ptr beanbag() const;

        /// Write details about something observed on this file system. After
        /// recording in the database it will build a packet and broadcast it to
        /// the other connected nodes
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            packet_builder);
        /// Write details about something observed on this file system. This version
        /// allows for custom hashing of the item if its added to the database
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            packet_builder, hasher_function);


        /// Record a change that has come from another server
        void remote_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            const tick &priority);

        /// The inodes
        const tree &inodes() const {
            return *inodes_p;
        }
        tree &inodes() {
            return *inodes_p;
        }

    private:
        std::unique_ptr<tree> inodes_p;
    };


}

