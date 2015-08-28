/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "notification.hpp"
#include <rask/subscriber.hpp>
#include <rask/tenant.hpp>


void rask::rm_inode(
    workers &w, tenant &tenant, const boost::filesystem::path &path
) {
    if ( !tenant.subscription ) {
        throw fostlib::exceptions::null(
            "Calling rm_inode on a tenant without a subscription",
            tenant.name());
    }
    rm_inode(w, *tenant.subscription, path);
}


void rask::rm_inode(
    workers &w, subscriber &sub, const boost::filesystem::path &path
) {
    sub.local_change(path, tenant::move_inode_out,
        move_out_packet);
}

