/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "notification.hpp"
#include "sweep.folder.hpp"
#include <rask/tenant.hpp>
#include <rask/workers.hpp>

#include <f5/fsnotify.hpp>
#include <f5/fsnotify/boost-asio.hpp>
#include <f5/fsnotify/fost.hpp>

#include <fost/log>

#include <map>


namespace {
    fostlib::threadsafe_store<
        std::pair<std::shared_ptr<rask::tenant>, boost::filesystem::path>, int>
            g_watches;


    struct callback : public f5::boost_asio::reader {
        rask::workers &w;

        callback(rask::workers &w)
        : reader(w.low_latency.io_service), w(w) {
        }

        void process(const inotify_event &event) {
            std::shared_ptr<rask::tenant> tenant;
            boost::filesystem::path parent;
            std::tie(tenant, parent) = g_watches.find(event.wd)[0];
            boost::filesystem::path name;
            if ( event.len ) {
                name = boost::filesystem::path(event.name);
            }
            const boost::filesystem::path filename(parent / name);
            fostlib::log::debug()
                ("", "inotify_event")
                ("wd", "descriptor", event.wd)
                ("wd", "directory", parent)
                ("wd", "pathname", filename)
                ("name", name)
                ("mask", f5::mask_json(event))
                ("cookie", event.cookie);

            if ( event.mask & IN_CREATE ) {
                if ( is_directory(filename) ) {
                    w.high_latency.io_service.post(
                        [this, filename, tenant]() {
                            rask::start_sweep(w, tenant, filename);
                        });
                }
            } else if ( event.mask & IN_DELETE_SELF ) {
                w.high_latency.io_service.post(
                    [this, filename = std::move(filename), tenant]() {
                        rask::rm_directory(w, tenant, filename);
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


rask::notification::~notification() {
}


void rask::notification::operator () () {
    pimpl->notifications();
}


bool rask::notification::watch(std::shared_ptr<tenant> tenant, const boost::filesystem::path &folder) {
    bool watched = false;
    pimpl->notifications.watch(folder.c_str(),
        [this, &watched, tenant, &folder](int wd) {
            watched = true;
            if ( g_watches.find(wd).size() == 0 ) {
                g_watches.add(wd, std::make_pair(tenant, folder));
                fostlib::log::debug()
                    ("", "Watch added")
                    ("wd", wd)
                    ("tenant", tenant->name())
                    ("directory", folder);
            } else {
                fostlib::log::debug()
                    ("", "Already watching")
                    ("wd", wd)
                    ("tenant", tenant->name())
                    ("directory", folder);
            }
        },
        [](){});
    return watched;
}

