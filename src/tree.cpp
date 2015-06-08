/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "tree.hpp"

#include <fost/log>


rask::tree::tree(fostlib::json c, fostlib::jcursor r, fostlib::jcursor h)
: root_db_config(std::move(c)), root(std::move(r)), name_hash_path(std::move(h)) {
}


beanbag::jsondb_ptr rask::tree::root_dbp() const {
    return beanbag::database(root_db_config);
}

