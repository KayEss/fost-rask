/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "sweep.inodes.hpp"
#include "tree.hpp"
#include <rask/subscriber.hpp>
#include <rask/tenant.hpp>

#include <f5/threading/eventfd.hpp>
#include <fost/counter>
#include <fost/log>


namespace {
    fostlib::performance p_directory(rask::c_fost_rask, "inode", "directory");
    fostlib::performance p_file(rask::c_fost_rask, "inode", "file");
    fostlib::performance p_move_out(rask::c_fost_rask, "inode", "move-out");
    fostlib::performance p_unknown(rask::c_fost_rask, "inode", "unknown");

    struct closure {
        std::shared_ptr<rask::tenant> tenant;
        boost::filesystem::path folder;
        rask::tree::const_iterator position;
        rask::tree::const_iterator end;
        beanbag::jsondb_ptr leaf_dbp;

        closure(std::shared_ptr<rask::tenant> t, boost::filesystem::path f)
        : tenant(t), folder(std::move(f)),
            position(t->subscription->inodes().begin()),
            end(t->subscription->inodes().end())
        {
            leaf_dbp = position.leaf_dbp();
        }
    };
    void check_block(
        rask::workers &w, std::shared_ptr<closure> c,
        f5::eventfd::limiter &limit, boost::asio::yield_context &yield
    ) {
        for ( ; c->position != c->end; ++c->position ) {
            auto inode = *c->position;
            auto filetype = inode["filetype"];
            auto filename = fostlib::coerce<boost::filesystem::path>(c->position.key());
            fostlib::log::debug(rask::c_fost_rask, "Inode sweep", inode);
            if ( filetype == rask::tenant::directory_inode ) {
                ++p_directory;
                w.notify.watch(c->tenant, filename);
            } else if ( filetype == rask::tenant::file_inode ) {
                ++p_file;
                auto task(++limit);
                rask::rehash_file(w, *c->tenant->subscription, filename, inode,
                    [task] () {
                        task->done(
                            [](const auto &error, auto bytes) {
                                fostlib::log::error(rask::c_fost_rask)
                                    ("", "Whilst notifying parent task that this one has completed.")
                                    ("error", error.message().c_str())
                                    ("bytes", bytes);
                            });
                    });
            } else if ( filetype == rask::tenant::move_inode_out ) {
                ++p_move_out;
            } else {
                ++p_unknown;
                fostlib::log::error(rask::c_fost_rask)
                    ("", "Sweeping inodes -- unknown filetype")
                    ("filetype", filetype)
                    ("inode", inode);
            }
            auto cpdb = c->position.leaf_dbp();
            if ( not(c->leaf_dbp == cpdb) ) {
                rehash_inodes(w, c->leaf_dbp);
                c->leaf_dbp = cpdb;
            }
        }
        rehash_inodes(w, c->leaf_dbp);
    }
}


void rask::sweep_inodes(
    workers &w, std::shared_ptr<tenant> t, boost::filesystem::path f
) {
    if ( !t->subscription ) {
        throw fostlib::exceptions::null(
            "Called sweep_inodes with a tenant that has no subscription");
    }
    auto c = std::make_shared<closure>(t, std::move(f));
    w.files.get_io_service().post(
        [&w, c]() {
            boost::asio::spawn(w.files.get_io_service(),
                [&w, c](boost::asio::yield_context yield) {
                    f5::eventfd::limiter limit(w.files.get_io_service(), yield, 2);
                    check_block(w, c, limit, yield);
                });
        });
}

