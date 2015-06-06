/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/connection.hpp>
#include <rask/workers.hpp>

#include <beanbag/beanbag>


namespace rask {


    /// Check the configuration for changes in tenants
    void tenants(workers &, const fostlib::json &dbconfig);

    /// The type of the current tenants
    typedef std::vector<fostlib::string> tenants_type;

    /// Return all of the current tenants
    tenants_type all_tenants();

    /// The type of a function used to build inode packets
    typedef std::function<
        connection::out(tenant &, const rask::tick &, const fostlib::string &)> packet_builder;

    /// Tenant working data
    class tenant {
        /// Used for internal caclulations
        const fostlib::string root;
    public:
        tenant(const fostlib::string &name, const fostlib::json &configuration);

        /// A directory inode
        static const fostlib::json directory_inode;
        /// An inode removal
        static const fostlib::json move_inode_out;

        /// The tenant's name
        const fostlib::accessors<fostlib::string> name;
        /// The tenant's configuration
        const fostlib::accessors<fostlib::json> configuration;
        /// The tenant beanbag
        beanbag::jsondb_ptr beanbag() const;
        /// The local filesystem path
        const fostlib::accessors<boost::filesystem::path> local_path;

        /// The current hash
        std::atomic<std::array<unsigned char, 32>> hash;

        /// Write details about something observed on this file system
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            packet_builder);
        /// Record a change that has come from another server
        void remote_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            const tick &priority);

        /// Tell the tenant about a directory in the observed file system
        void dir_stat(const boost::filesystem::path &location);
        /// Tell the tenant about a file in the observed file system
        void file_stat(const boost::filesystem::path &location,
            uintmax_t size, const std::time_t &modified, bool changed);
    };

    /// Return in-memory description of tenant -- empty if unknown
    std::shared_ptr<tenant> known_tenant(const fostlib::string &);


}

