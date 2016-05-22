/*
    Copyright 2015-2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include "subscriber.hpp"
#include "sweep.folder.hpp"
#include <rask/configuration.hpp>
#include <rask/tenant.hpp>

#include <f5/threading/eventfd.hpp>
#include <fost/counter>
#include <fost/log>


namespace {
    fostlib::performance p_starts(rask::c_fost_rask, "sweep", "started");
    fostlib::performance p_completed(rask::c_fost_rask, "sweep", "completed");
    fostlib::performance p_found_folders(rask::c_fost_rask, "sweep", "found", "folders");
    fostlib::performance p_found_files(rask::c_fost_rask, "sweep", "found", "files");
    fostlib::performance p_found_others(rask::c_fost_rask, "sweep", "found", "others");

    void sweep(
        rask::workers &w, std::shared_ptr<rask::tenant> tenant,
        boost::filesystem::path folder
    ) {
        if ( !tenant->subscription ) {
            throw fostlib::exceptions::null(
                "Trying to sweep a tenant that has no subscription");
        }
        boost::asio::spawn(w.files.get_io_service(),
            [&w, tenant, folder = std::move(folder)](boost::asio::yield_context yield) {
                ++p_starts;
                f5::eventfd::limiter limit(w.hashes.get_io_service(), yield, 8);
                if ( !boost::filesystem::is_directory(folder) ) {
                    throw fostlib::exceptions::not_implemented(
                        "Trying to recurse into a non-directory",
                        fostlib::coerce<fostlib::string>(folder));
                }
                fostlib::log::debug(rask::c_fost_rask, "Sweep recursing into folder", folder);
                tenant->subscription->directory(folder)
                    .broadcast(rask::create_directory_out)
                    .execute();
                w.notify.watch(tenant, folder);
                std::size_t files = 0, directories = 0, ignored = 0;
                using d_iter = boost::filesystem::recursive_directory_iterator;
                for ( auto inode = d_iter(folder), end = d_iter(); inode != end; ++inode ) {
                    fostlib::log::debug(rask::c_fost_rask, "Directory sweep", inode->path());
                    if ( inode->status().type() == boost::filesystem::directory_file ) {
                        ++directories; ++p_found_folders;
                        auto directory = inode->path();
                        tenant->subscription->directory(directory)
                            .broadcast(rask::create_directory_out)
                            .execute();
                        w.notify.watch(tenant, directory);
                    } else if ( inode->status().type() == boost::filesystem::regular_file ) {
                        ++files; ++p_found_files;
                        auto filename = inode->path();
                        auto task(++limit);
                        tenant->subscription->file(filename)
                            .hash([](const auto &, const auto &) {
                                /// We don't have the hash yet so leave blank for now
                                return fostlib::json();
                            })
                            .broadcast(rask::file_exists_out)
                            .post_commit([&w, task](const auto &c) {
                                rask::rehash_file(w, c.subscription, c.location, c.inode,
                                    [task] () {
                                        task->done(
                                            [](const auto &error, auto bytes) {
                                                fostlib::log::error(rask::c_fost_rask)
                                                    ("", "Whilst notifying parent task "
                                                        "that this one has completed.")
                                                    ("error", error.message().c_str())
                                                    ("bytes", bytes);
                                            });
                                    });
                            })
                        .execute();
                    } else {
                        ++ignored; ++p_found_others;
                    }
                }
                ++p_completed;
                fostlib::log::info(rask::c_fost_rask)
                    ("", "Swept folder")
                    ("folder", folder)
                    ("directories", directories)
                    ("files", files)
                    ("ignored", ignored);
            });
    }
}


void rask::start_sweep(
    workers &w, std::shared_ptr<tenant> tenant, boost::filesystem::path folder
) {
    w.files.get_io_service().post(
        [&w, tenant, folder = std::move(folder)]() {
            sweep(w, tenant, folder);
        });
}

