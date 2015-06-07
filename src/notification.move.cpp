/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "notification.hpp"
#include <rask/tenant.hpp>


void rask::rm_directory(
    workers &w, std::shared_ptr<tenant> tenant, const boost::filesystem::path &path
) {
    tenant->local_change(path, tenant::move_inode_out, move_out_packet);
}

