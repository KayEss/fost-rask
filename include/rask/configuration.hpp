/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/core>


namespace rask {


    /// The fost-rask module
    extern const fostlib::module c_fost_rask;


    /// The peering database to use
    extern const fostlib::setting<fostlib::json> c_peers_db;
    /// The server database to use
    extern const fostlib::setting<fostlib::json> c_server_db;
    /// The tenants database to use
    extern const fostlib::setting<fostlib::json> c_tenant_db;

    /// Set this to true if you want rask to terminate after the reactor pool
    /// catches any exception. If false then rask will attempt to carry on.
    /// When terminating the log queue is flushed before `std::terminate` is
    /// called.
    extern const fostlib::setting<bool> c_terminate_on_exception;


}

