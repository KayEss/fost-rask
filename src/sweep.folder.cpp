/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "sweep.folder.hpp"
#include <rask/tenants.hpp>

#include <fost/log>


void rask::start_sweep(workers &w, std::shared_ptr<tenant> tenant, boost::filesystem::path folder) {
    if ( !boost::filesystem::is_directory(folder) ) {
        throw fostlib::exceptions::not_implemented(
            "Trying to recurse into a non-directory",
            fostlib::coerce<fostlib::string>(folder));
    }
    fostlib::log::debug("Sweep recursing into folder", folder);
    auto watched = w.notify.watch(tenant, folder);
    std::size_t files = 0, directories = 0, ignored = 0;
    typedef boost::filesystem::directory_iterator d_iter;
    for ( auto inode = d_iter(folder), end = d_iter(); inode != end; ++inode ) {
        fostlib::log::debug("Hit inode", inode->path());
        if ( inode->status().type() == boost::filesystem::directory_file ) {
            ++directories;
            w.high_latency.io_service.post(
                [&w, filename = inode->path(), tenant]() {
                    start_sweep(w, tenant, filename);
                });
            tenant->dir_stat(inode->path());
        }
    }
    fostlib::log::info()
        ("", "Swept folder")
        ("folder", folder)
        ("directories", directories)
        ("files", files)
        ("ignored", ignored)
        ("watched", watched);
}

