/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "sweep.folder.hpp"
#include <rask/configuration.hpp>
#include <rask/configuration.hpp>
#include <rask/tenant.hpp>

#include <fost/counter>
#include <fost/log>

#include <boost/asio/spawn.hpp>


namespace {
    fostlib::performance p_starts(rask::c_fost_rask, "sweep", "started");
    fostlib::performance p_completed(rask::c_fost_rask, "sweep", "completed");
    fostlib::performance p_swept(rask::c_fost_rask, "sweep", "folders");
    fostlib::performance p_recursing(rask::c_fost_rask, "sweep", "recursing");
    fostlib::performance p_recursed(rask::c_fost_rask, "sweep", "recursed");
    fostlib::performance p_paused(rask::c_fost_rask, "sweep", "pauses");

    struct limiter {
        boost::asio::posix::stream_descriptor fd;
        std::atomic<uint64_t> outstanding;
        limiter(rask::workers &w, int f)
        : fd(w.high_latency.io_service, f), outstanding(0) {
            ++p_starts;
        }
        ~limiter() {
            ++p_completed;
        }
    };

    void sweep(
        rask::workers &w, std::shared_ptr<rask::tenant> tenant,
        boost::filesystem::path folder, std::shared_ptr<limiter> limit
    ) {
        boost::asio::spawn(w.high_latency.io_service,
            [&w, tenant, folder, limit](boost::asio::yield_context yield) {
                ++p_swept;
                if ( !boost::filesystem::is_directory(folder) ) {
                    throw fostlib::exceptions::not_implemented(
                        "Trying to recurse into a non-directory",
                        fostlib::coerce<fostlib::string>(folder));
                }
                tenant->local_change(
                    folder, rask::tenant::directory_inode, rask::create_directory_out);
                fostlib::log::debug(rask::c_fost_rask, "Sweep recursing into folder", folder);
                auto watched = w.notify.watch(tenant, folder);
                std::size_t files = 0, directories = 0, ignored = 0;
                using d_iter = boost::filesystem::directory_iterator;
                for ( auto inode = d_iter(folder), end = d_iter(); inode != end; ++inode ) {
                    if ( inode->status().type() == boost::filesystem::directory_file ) {
                        ++limit->outstanding;
                        ++directories;
                        ++p_recursing;
                        w.high_latency.io_service.post(
                            [&w, filename = inode->path(), tenant, limit]() {
                                ++p_recursed;
                                uint64_t count = 1;
                                boost::asio::async_write(limit->fd,
                                    boost::asio::buffer(&count, sizeof(count)),
                                    [](const boost::system::error_code &error, std::size_t bytes) {
                                        if ( error || bytes != sizeof(count) ) {
                                            fostlib::log::error(rask::c_fost_rask)
                                                ("", "Whilst notifying parent task that this one has started.")
                                                ("error", error.message().c_str())
                                                ("bytes", bytes);
                                        }
                                    });
                                sweep(w, tenant, filename, limit);
                            });
                    }
                    while ( limit->outstanding > 16 ) {
                        uint64_t count = 0;
                        ++p_paused;
                        boost::asio::streambuf buffer;
                        boost::asio::async_read(limit->fd, buffer,
                            boost::asio::transfer_exactly(sizeof(count)), yield);
                        buffer.sgetn(reinterpret_cast<char *>(&count), sizeof(count));
                        if ( count > limit->outstanding.load() )
                            throw fostlib::exceptions::out_of_range<uint64_t>(
                                "Just completed jobs is higher than the outsanding number",
                                0, limit->outstanding.load(), count);
                        limit->outstanding -= count;
                        fostlib::log::debug(rask::c_fost_rask)
                            ("", "Rate limit on rask::sweep_folder")
                            ("outstanding", limit->outstanding.load())
                            ("just-completed", count);
                    }
                }
                fostlib::log::info(rask::c_fost_rask)
                    ("", "Swept folder")
                    ("folder", folder)
                    ("directories", directories)
                    ("files", files)
                    ("ignored", ignored)
                    ("watched", watched);
            });
    }
}


void rask::start_sweep(
    workers &w, std::shared_ptr<tenant> tenant, boost::filesystem::path folder
) {
    int fd(eventfd(0, 0));
    sweep(w, tenant, folder, std::make_shared<limiter>(w, fd));
}

