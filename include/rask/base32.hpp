/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/string>


namespace rask {


    /// Encoding and checking of the base32 strings
    struct base32_string_tag {
        static const char characters[];
        static void do_encode(fostlib::nliteral, fostlib::ascii_string &);
        static void do_encode(const fostlib::ascii_string &, fostlib::ascii_string &);
        static void check_encoded(const fostlib::ascii_string &);
    };
    /// A base 32 like string, but omitting i, j, l and o.
    typedef fostlib::tagged_string<base32_string_tag, fostlib::ascii_string> base32_string;


    /// Return the number that is represented by the single base32 digit
    int8_t from_base32_ascii_digit(fostlib::utf32);
    /// Return the digit represents the single number
    fostlib::utf32 to_base32_ascii_digit(uint8_t);


}


namespace fostlib {
    /// Allow a base32 string to be generated from a vector of byte values
    template<>
    struct coercer<rask::base32_string, std::vector< unsigned char >> {
        /// Perform the coercion
        rask::base32_string coerce(const std::vector<unsigned char>&);
    };

    /// Allow integers to be converted to a padded base32 string
    template<typename I>
    struct coercer<rask::base32_string, I> {
        /// Perform the coercion
        rask::base32_string coerce(I number) {
            std::string s;
            for ( std::size_t bits{}; bits < sizeof(I) * 8; bits += 5 ) {
                s.insert(0, 1, rask::to_base32_ascii_digit(number & 31));
                number /= 32;
            }
            return fostlib::ascii_string(s);
        }
    };

    /// Allow coerceon to a string
    template<>
    struct coercer< string, rask::base32_string > {
        string coerce( const rask::base32_string &h ) {
            return fostlib::coerce<string>(h.underlying());
        }
    };
}

