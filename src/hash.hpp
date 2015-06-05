/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/jsondb>


namespace rask {


    class tenant;


    /// Re-hash starting at the inode list level
    void rehash_inodes(const tenant &, const fostlib::jsondb::local &);

    /// Re-hash starting at the tenants level
    void rehash_tenants(const fostlib::jsondb::local &);


}

