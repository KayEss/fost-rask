/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include "notification.hpp"
#include "sweep.folder.hpp"
#include <rask/tenant.hpp>
#include <rask/workers.hpp>

#include <f5/fsnotify.hpp>
#include <f5/fsnotify/boost-asio.hpp>
#include <f5/fsnotify/fost.hpp>
#include <f5/threading/map.hpp>

#include <fost/counter>
#include <fost/log>

#include <map>


namespace {
    f5::tsmap<int,
        std::pair<std::shared_ptr<rask::tenant>, boost::filesystem::path>>
            g_watches;

    fostlib::performance p_watches(rask::c_fost_rask,
        "inotify", "watches");
    fostlib::performance p_watches_failed(rask::c_fost_rask,
        "inotify", "watches-failed");
    fostlib::performance p_in_create_dir(rask::c_fost_rask,
        "inotify", "event", "IN_CREATE", "directory");
    fostlib::performance p_in_create_file(rask::c_fost_rask,
        "inotify", "event", "IN_CREATE", "file");
    fostlib::performance p_in_create_other(rask::c_fost_rask,
        "inotify", "event", "IN_CREATE", "other");
    fostlib::performance p_in_modify(rask::c_fost_rask,
        "inotify", "event", "IN_MODIFY");
    fostlib::performance p_in_delete_self(rask::c_fost_rask,
        "inotify", "event", "IN_DELETE_SELF");

    struct callback : public f5::fsnotify::boost_asio::reader {
        rask::workers &w;

        callback(rask::workers &w)
        : reader(w.io.get_io_service()), w(w) {
        }

        void process(const inotify_event &event) {
            std::shared_ptr<rask::tenant> tenant;
            boost::filesystem::path parent;
            std::tie(tenant, parent) = g_watches.find(event.wd);
            if ( not tenant ) return;
            boost::filesystem::path name;
            if ( event.len ) {
                name = boost::filesystem::path(event.name);
            }
            const boost::filesystem::path filename(parent / name);
            fostlib::log::debug(rask::c_fost_rask)
                ("", "inotify_event")
                ("wd", "descriptor", event.wd)
                ("wd", "directory", parent)
                ("wd", "pathname", filename)
                ("name", name)
                ("mask", f5::mask_json(event))
                ("cookie", event.cookie);

            /// TODO: Eventually this needs to take care of all of the
            /// relevant events. We probably don't care about:
            /// * IN_ACCESS
            /// * IN_ATTRIB
            /// * IN_CLOSE_NOWRITE
            /// * IN_OPEN
            /// See: http://man7.org/linux/man-pages/man7/inotify.7.html
            if ( event.mask & IN_CREATE ) {
                if ( is_directory(filename) ) {
                    ++p_in_create_dir;
                    w.files.get_io_service().post(
                        [this, filename, tenant]() {
                            rask::start_sweep(w, tenant, filename);
                        });
                } else if ( is_regular_file(filename) ) {
                    ++p_in_create_file;
                } else {
                    ++p_in_create_other;
                }
            } else if ( event.mask & IN_MODIFY ) {
                ++p_in_modify;
                w.files.get_io_service().post(
                    [this, filename = std::move(filename), tenant]() {
                        rask::rehash_file(w, *tenant, filename);
                    });
            } else if ( event.mask & IN_DELETE_SELF ) {
                ++p_in_delete_self;
                w.files.get_io_service().post(
                    [this, filename = std::move(filename), tenant]() {
                        rask::rm_inode(w, *tenant, filename);
                    });
            }
        }
    };
}


struct rask::notification::impl {
    f5::notifications<callback> notifications;
    impl(workers &w)
    : notifications(w) {
    }
};


rask::notification::notification(workers &w)
: pimpl(new impl(w)) {
}


rask::notification::~notification() = default;


void rask::notification::operator () () {
    pimpl->notifications();
}


bool rask::notification::watch(
    std::shared_ptr<tenant> tenant, const boost::filesystem::path &folder
) {
    bool watched = false;
    pimpl->notifications.watch(folder.c_str(),
        [this, &watched, tenant, &folder](int wd) {
            watched = true;
            g_watches.add_if_not_found(wd, [tenant, &folder, wd]() {
                ++p_watches;
                fostlib::log::debug(c_fost_rask)
                    ("", "Watch added")
                    ("wd", wd)
                    ("tenant", tenant->name())
                    ("directory", folder);
                return std::make_pair(tenant, folder);
            });
        },
        [tenant, &folder]() {
            ++p_watches_failed;
            fostlib::log::error(c_fost_rask)
                ("", "Watch failed")
                ("check", "/proc/sys/fs/inotify/max_user_watches")
                ("tenant", tenant->name())
                ("directory", folder);
        });
    return watched;
}

