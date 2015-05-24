/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <beanbag/beanbag>
#include <fost/log>

#include <rask/tenants.hpp>


void rask::tenants(const fostlib::json &dbconfig) {
    fostlib::log::debug("Loading tennants database", dbconfig);
    beanbag::jsondb_ptr dbp(beanbag::database(dbconfig));
}

