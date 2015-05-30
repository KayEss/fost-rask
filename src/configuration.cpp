/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/configuration.hpp>


const fostlib::setting<fostlib::json> rask::c_peers_db(
    "fost-rask/configuration.cpp", "rask", "peers", fostlib::json(), true);
const fostlib::setting<fostlib::json> rask::c_server_db(
    "fost-rask/configuration.cpp", "rask", "server", fostlib::json(), true);
const fostlib::setting<fostlib::json> rask::c_tenant_db(
    "fost-rask/configuration.cpp", "rask", "tenants", fostlib::json(), true);

