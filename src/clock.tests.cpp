/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/clock.hpp>
#include <fost/test>


FSL_TEST_SUITE(clock);


FSL_TEST_FUNCTION(size) {
    FSL_CHECK_EQ(sizeof(int64_t), 8u);
    FSL_CHECK_EQ(sizeof(int32_t), 4u);
    FSL_CHECK_EQ(sizeof(rask::tick), 16u);
}

