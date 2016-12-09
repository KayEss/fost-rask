/*
    Copyright 2015-2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/core>


namespace rask {


    /// The fost-rask module
    extern const fostlib::module c_fost_rask;


    /// Use a 32KB block size for hashing a file
    const std::size_t file_hash_block_size = 32 * 1024;


    /// The peering database to use
    extern const fostlib::setting<fostlib::json> c_peers_db;
    /// The server database to use
    extern const fostlib::setting<fostlib::json> c_server_db;
    /// The tenants database to use
    extern const fostlib::setting<fostlib::json> c_tenant_db;
    /// The subscriptions database to use
    extern const fostlib::setting<fostlib::json> c_subscriptions_db;

    /// Set this to true if you want rask to terminate after the reactor pool
    /// catches any exception. If false then rask will attempt to carry on.
    /// When terminating the log queue is flushed before `std::terminate` is
    /// called.
    extern const fostlib::setting<bool> c_terminate_on_exception;

    /// When true exit after the version packet exchange shows that the
    /// server is in sync with at least one peer. This is primarily intended
    /// for some types of performance testing between two peers
    extern const fostlib::setting<bool> c_exit_on_sync_success;


}

