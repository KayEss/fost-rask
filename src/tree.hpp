/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <beanbag/beanbag>


namespace rask {


    /// Class that can be used to provide an interface onto the beanbags
    /// needed to implement the tree for large collections of names -- tenants
    /// and inodes.
    class tree {
        /// The beanbag configuration for the root database
        const fostlib::json root_db_config;
        /// The root jcursor that we are going to iterate on
        const fostlib::jcursor root;
        /// The path within an entry where we can find the hash
        const fostlib::jcursor name_hash_path;
    public:
        /// Construct the tree
        tree(fostlib::json config, fostlib::jcursor root, fostlib::jcursor hash);

        /// Return the root database
        beanbag::jsondb_ptr root_dbp() const;
    };


}

