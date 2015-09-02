/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "subscriber.hpp"
#include <rask/tenant.hpp>
#include <rask/workers.hpp>
#include <fost/test>


using namespace fostlib;


FSL_TEST_SUITE(tenant);


FSL_TEST_FUNCTION(path_does_not_get_double_slash) {
    rask::workers w;
    fostlib::json conf;
    fostlib::insert(conf, "path", "m/");
    rask::tenant t(w, "t1", conf);
    FSL_CHECK(t.subscription.get());
    FSL_CHECK_EQ(t.subscription->local_path(), "m/");
}


FSL_TEST_FUNCTION(path_gets_slash) {
    rask::workers w;
    fostlib::json conf;
    fostlib::insert(conf, "path", "m");
    rask::tenant t(w, "t1", conf);
    FSL_CHECK(t.subscription.get());
    FSL_CHECK_EQ(t.subscription->local_path(), "m/");
}

