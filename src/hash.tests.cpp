/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include <fost/test>


FSL_TEST_SUITE(hash);


FSL_TEST_FUNCTION(size) {
    FSL_CHECK_EQ(sizeof(rask::file::block_hash), 32u);
}


FSL_TEST_FUNCTION(name_hash) {
    fostlib::string fs(L"ab\u2014xx");
    boost::filesystem::path bfp("ab\xE2\x80\x94xx");
    FSL_CHECK_EQ(rask::name_hash(fs), "25paxrth735bymjay86cd900ye");
    FSL_CHECK_EQ(rask::name_hash(fs), rask::name_hash(bfp));
}


FSL_TEST_FUNCTION(filepath) {
    FSL_CHECK_EQ(rask::name_hash_path("ab"), "ab");
    FSL_CHECK_EQ(rask::name_hash_path("abc"), "ab/c");
    FSL_CHECK_EQ(rask::name_hash_path("abcdef"), "ab/cd/ef");
    FSL_CHECK_EQ(
        rask::name_hash_path("abcdefghijklmnopqrstuvw"),
        "ab/cd/efg/hijk/lmnop/qrstuv/w");
}

