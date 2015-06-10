/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/base32.hpp>

#include <fost/exception/parse_error.hpp>


using namespace fostlib;


// Choose the same symbols as Douglas Crockford, but lower case
// http://www.crockford.com/wrmg/base32.html
const char rask::base32_string_tag::characters[] =
    "0123456789abcdefghjkmnpqrstvwxyz";


void rask::base32_string_tag::do_encode(fostlib::nliteral l, fostlib::ascii_string &s) {
    s = l;
    check_encoded(s);
}
void rask::base32_string_tag::do_encode(
    const fostlib::ascii_string &l, fostlib::ascii_string &s
) {
    s = l;
    check_encoded(s);
}


void rask::base32_string_tag::check_encoded(const fostlib::ascii_string &s) {
    if ( s.underlying().find_first_not_of(
            rask::base32_string_tag::characters) != std::string::npos )
        throw fostlib::exceptions::parse_error(
            "Non base32 character found in string", coerce< string >(s));
}


rask::base32_string fostlib::coercer<
    rask::base32_string, std::vector<unsigned char>
>::coerce(const std::vector<unsigned char> &v) {
    std::string string;
    if ( v.size() ) {
        auto position = v.rbegin();
        uint16_t number = *position++;
        int bits = 8;
        do {
            string.insert(0, 1, rask::base32_string_tag::characters[number & 31]);
            number >>= 5; bits -= 5;
            if ( bits < 8 && position != v.rend() ) {
                number += uint16_t(*position++) << bits;
                bits += 8;
            }
        } while ( string.size() < v.size() * 8 / 5 + 1 );
    }
    return fostlib::ascii_string(string);
}
