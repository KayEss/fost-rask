/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/core>


namespace rask {


    /// A tick of the clock
    class tick {
        /// Construct a local tick
        explicit tick(int64_t);

    public:
        /// The most significant part of the time
        const int64_t time;
        /// The server identity used as a tie breaker
        const int32_t server;
        /// Reserved to ensure we get 16 good bytes
        const int32_t reserved;

        /// Return the current time
        static tick now();
        /// A Lamport clock used to give each event a unique ID
        static tick next();
        /// Update the clock here if required
        static void overheard(int64_t, int32_t);
    };


}


namespace fostlib {

    template<>
    struct coercer<json, rask::tick> {
        json coerce(rask::tick);
    };


}

