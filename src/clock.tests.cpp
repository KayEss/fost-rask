/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/clock.hpp>
#include <fost/push_back>
#include <fost/test>


FSL_TEST_SUITE(clock);


FSL_TEST_FUNCTION(size) {
    FSL_CHECK_EQ(sizeof(int64_t), 8u);
    FSL_CHECK_EQ(sizeof(int32_t), 4u);
    FSL_CHECK_EQ(sizeof(rask::tick), 16u);
}


FSL_TEST_FUNCTION(equality) {
    fostlib::json t;
    fostlib::push_back(t, 1);
    fostlib::push_back(t, 2);
    rask::tick t1(t);
    rask::tick t2(t);
    FSL_CHECK_EQ(t1, t2);
    FSL_CHECK(std::memcmp(&t1, &t2, sizeof(rask::tick)) == 0);
}


FSL_TEST_FUNCTION(ordering) {
    FSL_CHECK(rask::tick(0, 0) < rask::tick(1, 0));
    FSL_CHECK(rask::tick(0, 0) < rask::tick(0, 1));
}


FSL_TEST_FUNCTION(hash) {
    fostlib::json t;
    fostlib::push_back(t, 1);
    fostlib::push_back(t, 2);
    fostlib::digester d1(fostlib::sha256), d2(fostlib::sha256);
    rask::tick t1(t);
    rask::tick t2(t);
    FSL_CHECK(std::memcmp(&t1, &t2, sizeof(rask::tick)) == 0);
    d1 << t1;
    d2 << t2;
    FSL_CHECK_EQ(
        fostlib::coerce<fostlib::hex_string>(d1.digest()),
        fostlib::coerce<fostlib::hex_string>(d2.digest()));
}

