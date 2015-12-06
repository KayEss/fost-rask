/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/crypto>


namespace rask {


    /// A tick of the clock
    class tick {
        /// Construct a local tick
        explicit tick(int64_t);
        /// Construct any tick
        tick(int64_t, uint32_t);

    public:
        /// Allow copying
        tick(const tick &) = default;
        /// Construct a clock from JSON
        tick(const fostlib::json &);

        /// The most significant part of the time
        const int64_t time;
        /// The server identity used as a tie breaker
        const uint32_t server;
        /// Reserved to ensure we get 16 good bytes
        const uint32_t reserved;

        /// Compare two ticks
        bool operator < (const tick &t) const {
            if ( time < t.time )
                return true;
            else if ( time == t.time )
                return server < t.server;
            else
                return false;
        }

        /// Compare for equality
        bool operator == (const tick &t) const {
            return time == t.time && server == t.server && reserved == t.reserved;
        }

        /// Return an advanced tick relative to this one
        tick operator + ( int64_t amount ) {
            return tick(time + amount, 0u);
        }

        /// Return the current time with optional hash
        static std::pair<tick, fostlib::nullable<fostlib::string>> now();
        /// A Lamport clock used to give each event a unique ID
        static tick next();
        /// Update the clock here if required
        static tick overheard(int64_t, uint32_t);
    };


}


namespace fostlib {


    /// Allow coercion of the tick to JSON
    template<>
    struct coercer<json, rask::tick> {
        json coerce(const rask::tick &);
    };


    digester &operator << (digester &, const rask::tick &);


}


namespace std {


    inline
    ostream &operator << (ostream &o, const rask::tick &t) {
        return o << '[' << t.time << ", " << t.server << ", " << t.reserved << ']';
    }


}


