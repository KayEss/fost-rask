/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/workers.hpp>


namespace rask {


    /// Check the configuration for changes in tenants
    void tenants(workers &, const fostlib::json &dbconfig);

    /// The type of the current tenants
    typedef std::vector<fostlib::string> tenants_type;

    /// Return all of the current tenants
    tenants_type all_tenants();

    /// Return JSON describing a tenant -- empty if unknown
    fostlib::json known_tenant(const fostlib::string &);


}

