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
        tick(int64_t);

    public:
        /// The most significant part of the time
        const int64_t time;
        /// The server identity used as a tie breaker
        const int32_t server;

        /// A Lamport clock used to give each event a unique ID
        static tick next();
    };


}


namespace fostlib {

    template<>
    struct coercer<json, rask::tick> {
        json coerce(rask::tick);
    };


}

