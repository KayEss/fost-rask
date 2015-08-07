/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/connection.hpp>
#include <rask/workers.hpp>


namespace rask {


    class subscriber;
    class tree;
    struct workers;


    /// Check the configuration for changes in tenants
    void tenants(workers &,
        const fostlib::json &tenant_config,
        const fostlib::json &subscribers_config);

    /// The type of the current tenants
    typedef std::vector<fostlib::string> tenants_type;

    /// Return all of the current tenants
    tenants_type all_tenants();

    /// Tenant working data
    class tenant {
    public:
        /// Construct a tenant representation that can't have a subscription
        tenant(workers &, const fostlib::string &name);
        /// Construct a tenant representation with optional subscription
        tenant(workers &, const fostlib::string &name, const fostlib::json &configuration);
        /// Explicit desctructor
        ~tenant();

        /// A directory inode
        static const fostlib::json directory_inode;
        /// A normal file inode
        static const fostlib::json file_inode;
        /// An inode removal
        static const fostlib::json move_inode_out;

        /// The tenant's name
        const fostlib::accessors<fostlib::string> name;
        /// The tenant's subscription configuration
        const fostlib::accessors<fostlib::json> configuration;
        /// The subscription (if present)
        const std::shared_ptr<subscriber> subscription;

        /// The current hash
        std::atomic<std::array<unsigned char, 32>> hash;
    };

    /// Return in-memory description of tenant
    std::shared_ptr<tenant> known_tenant(workers &,  const fostlib::string &);


}

