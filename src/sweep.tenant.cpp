/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "subscriber.hpp"
#include "sweep.folder.hpp"
#include "sweep.inodes.hpp"
#include "sweep.tenant.hpp"
#include <rask/configuration.hpp>
#include <rask/tenant.hpp>

#include <fost/log>


void rask::start_sweep(workers &w, std::shared_ptr<tenant> tenant) {
    if ( tenant->subscription ) {
        fostlib::log::info(c_fost_rask, "Starting sweep for", tenant->name(), tenant->configuration());
        w.files.get_io_service().post(
            [&w, tenant]() {
                sweep_inodes(w, tenant, tenant->subscription->local_path());
                start_sweep(w, tenant, tenant->subscription->local_path());
            });
    }
}

