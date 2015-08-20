/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/base32.hpp>
#include <fost/test>


using namespace fostlib;


FSL_TEST_SUITE(base32);


FSL_TEST_FUNCTION(coercion) {
    typedef std::vector<unsigned char> v;
    FSL_CHECK_EQ(coerce<rask::base32_string>(v()), "");
    FSL_CHECK_EQ(coerce<rask::base32_string>(v{0}), "00");
    FSL_CHECK_EQ(coerce<rask::base32_string>(v{31}), "0z");
    FSL_CHECK_EQ(coerce<rask::base32_string>(v{32}), "10");
    FSL_CHECK_EQ(coerce<rask::base32_string>(v{0x04, 0xd2}), "016j");
    FSL_CHECK_EQ(coerce<rask::base32_string>(v{0x01, 0xe2, 0x40}), "03rj0");
    FSL_CHECK_EQ(coerce<rask::base32_string>(v{0xff, 0xff, 0xff}), "fzzzz");
    FSL_CHECK_EQ(coerce<rask::base32_string>(v{0x01, 0xe2, 0x40, 0x42}), "00y4g22");
}


FSL_TEST_FUNCTION(digit) {
    FSL_CHECK_EQ(rask::from_base32_ascii_digit('0'), 0);
    FSL_CHECK_EQ(rask::from_base32_ascii_digit('9'), 9);
    FSL_CHECK_EQ(rask::from_base32_ascii_digit('a'), 10);
    FSL_CHECK_EQ(rask::from_base32_ascii_digit('z'), 31);
}


FSL_TEST_FUNCTION(integers) {
    FSL_CHECK_EQ(coerce<rask::base32_string>(int8_t(0)), "00");
    FSL_CHECK_EQ(coerce<rask::base32_string>(int16_t(0)), "0000");
    FSL_CHECK_EQ(coerce<rask::base32_string>(int32_t(0)), "0000000");
    FSL_CHECK_EQ(coerce<rask::base32_string>(int16_t(0x04d2)), "016j");
    FSL_CHECK_EQ(coerce<rask::base32_string>(int32_t(0x01e24042)), "00y4g22");
}

