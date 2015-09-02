/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "notification.hpp"
#include "subscriber.hpp"
#include <rask/tenant.hpp>


void rask::inode_exists(
    workers &w, subscriber &sub, const boost::filesystem::path &path
) {
    if ( boost::filesystem::is_directory(path) ) {
        sub(path, tenant::directory_inode)
            .broadcast(create_directory_out)
            .execute();
    } else if ( boost::filesystem::is_regular_file(path) ) {
        sub(path, tenant::file_inode)
            .broadcast(file_exists_out)
            .execute();
    } else
        throw fostlib::exceptions::not_implemented(
            "Cannot set that an inode exists when it isn't a file or directory",
            fostlib::coerce<fostlib::string>(path));
}


void rask::inode_changed(
    workers &w, subscriber &sub, const boost::filesystem::path &path
) {
    if ( boost::filesystem::is_regular_file(path) ) {
        /// TODO We need to kick off a change into the database and
        /// post process other things
    } else
        throw fostlib::exceptions::not_implemented(
            "Cannot react to an inode change when it isn't a file",
            fostlib::coerce<fostlib::string>(path));
}
