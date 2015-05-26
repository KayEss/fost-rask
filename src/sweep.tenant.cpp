/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "sweep.tenant.hpp"

#include <fost/log>


void rask::start_sweep(const fostlib::string &tenant, const fostlib::json &config) {
    fostlib::log::info("Starting sweep for", tenant, config);
}

