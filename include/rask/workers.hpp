/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/pool.hpp>


namespace rask {


    /// Stores the workers in a way that they can be passed around
    struct workers {
        /// Construct the pools
        workers();
        /// Don't allow copying
        workers(const workers &) = delete;
        workers &operator = (const workers &) = delete;

        /// Worker pool for IO related tasks (i.e. low latency to react)
        pool low_latency;
        /// Worker pool for longer running tasks
        pool high_latency;
    };


}
