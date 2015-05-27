/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "sweep.folder.hpp"

#include <fost/log>


void rask::start_sweep(workers &w, const boost::filesystem::path &folder) {
    fostlib::log::debug("Sweep recursing into folder", folder);
    auto watched = w.notify.watch(folder);
    std::size_t files = 0, directories = 0, ignored = 0;
    typedef boost::filesystem::directory_iterator d_iter;
    for ( auto inode = d_iter(folder); inode != d_iter(); ++inode ) {
        if ( inode->status().type() == boost::filesystem::directory_file ) {
            ++directories;
            w.high_latency.io_service.post([&w, folder = inode->path()]() {
                start_sweep(w, folder);
            });
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

