/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <fost/jsondb>


namespace {
    const fostlib::setting<fostlib::string> c_data_root(
        "rask/configuration.tests.cpp", fostlib::c_jsondb_root,
        fostlib::coerce<fostlib::string>(fostlib::unique_filename()));
}
