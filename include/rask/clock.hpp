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

        int64_t t;
        uint32_t s, r;

    public:
        /// Default construct to zero
        tick()
        : t{0}, s{0}, r{0}
        {}
        /// Allow copying
        tick(const tick &) = default;
        /// Construct any tick
        tick(int64_t, uint32_t);
        /// Construct a clock from JSON
        tick(const fostlib::json &);

        /// The most significant part of the time
        int64_t time() const {
            return t;
        }
        /// The server identity used as a tie breaker
        uint32_t server() const {
            return s;
        }
        /// Reserved to ensure we get 16 good bytes
        uint32_t reserved() const {
            return r;
        };

        /// Compare two ticks
        bool operator < (const tick &o) const {
            if ( t < o.t )
                return true;
            else if ( t == o.t )
                return s < o.s;
            else
                return r < o.r;
        }

        /// Compare for equality
        bool operator == (const tick &o) const {
            return t == o.t && s == o.s && r == o.r;
        }

        /// Return an advanced tick relative to this one
        tick operator + ( int64_t amount ) const {
            return tick(t + amount, 0u);
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
        return o << '[' << t.time() << ", " << t.server() << ", " << t.reserved() << ']';
    }


}


