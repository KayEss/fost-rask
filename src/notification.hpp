/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include<boost/filesystem.hpp>


namespace rask {


    class subscriber;
    class tenant;
    struct workers;


    /// Record a locally observed removal of an inode
    void rm_inode(workers &, tenant &, const boost::filesystem::path &);
    void rm_inode(workers &, subscriber &, const boost::filesystem::path &);


}
