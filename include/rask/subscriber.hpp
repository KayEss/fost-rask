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
    using packet_builder = std::function<
        connection::out(tenant &, const rask::tick &, const fostlib::string &,
            const fostlib::json &)>;
    /// The type of the function that tells the subscription whether it needs to
    /// update or not
    using condition_function = std::function<bool(const fostlib::json &inode)>;
    /// The type of the function used to re-write the inode before it is saved
    using inode_function = std::function<
        fostlib::json(const rask::tick &, fostlib::json)>;
    /// The type of the function used to manipulate the database entry if
    /// the entry is not being replaced
    using otherwise_function = std::function<fostlib::json(fostlib::json)>;


    /// The part of the tenant that is a subscriber
    class subscriber {
    public:
        /// Used for internal caclulations
        const fostlib::string root;
        /// The tenant that this subscription is part of
        rask::tenant &tenant;

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
        /// allows for custom hashing of the item if it's added to the database
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            packet_builder, inode_function);
        /// Write details about something observed on this file system. This version
        /// allows for custom hashing and also a custom predicate to determine if
        /// the inode data entry needs to be updated
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            condition_function, packet_builder, inode_function);
        /// Write details about something observed on this file system. This
        /// version allows for custom hashing, a custom predicate to deterime if
        /// the inode data entry needs to be updated plus a secondary updater
        /// that allows other information in the inode to be updated if required.
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            condition_function, packet_builder, inode_function, otherwise_function);

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

