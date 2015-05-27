/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/workers.hpp>

#include <f5/fsnotify.hpp>
#include <f5/fsnotify/boost-asio.hpp>

#include <fost/log>
#include <fost/push_back>

#include <map>


namespace {
    fostlib::threadsafe_store<
        std::pair<std::shared_ptr<rask::tenant>, boost::filesystem::path>, int>
            g_watches;


    struct callback : public f5::boost_asio::reader {
        callback(boost::asio::io_service &s)
        : reader(s) {
        }

        void process(const inotify_event &event) {
            std::shared_ptr<rask::tenant> tenant;
            boost::filesystem::path parent;
            std::tie(tenant, parent) = g_watches.find(event.wd)[0];
            boost::filesystem::path name(event.name, event.name + event.len);
            fostlib::json mask;
            if ( event.mask & IN_IGNORED )
                fostlib::push_back(mask, "IN_IGNORED");
            if ( event.mask & IN_CREATE )
                fostlib::push_back(mask, "IN_CREATE");
            if ( event.mask & IN_OPEN )
                fostlib::push_back(mask, "IN_OPEN");
            if ( event.mask & IN_MODIFY )
                fostlib::push_back(mask, "IN_MODIFY");
            if ( event.mask & IN_CLOSE_NOWRITE )
                fostlib::push_back(mask, "IN_CLOSE_NOWRITE");
            if ( event.mask & IN_CLOSE_WRITE )
                fostlib::push_back(mask, "IN_CLOSE_WRITE");
            if ( event.mask & IN_DELETE )
                fostlib::push_back(mask, "IN_DELETE");
            if ( event.mask & IN_DELETE_SELF )
                fostlib::push_back(mask, "IN_DELETE_SELF");
            if ( event.mask & IN_MOVE_SELF )
                fostlib::push_back(mask, "IN_MOVE_SELF");
            if ( event.mask & IN_MOVED_FROM )
                fostlib::push_back(mask, "IN_MOVED_FROM");
            if ( event.mask & IN_MOVED_TO )
                fostlib::push_back(mask, "IN_MOVED_TO");
            if ( event.mask & IN_UNMOUNT )
                fostlib::push_back(mask, "IN_UNMOUNT");
            fostlib::log::debug()
                ("", "inotify_event")
                ("wd", "descriptor", event.wd)
                ("wd", "directory", parent)
                ("wd", "pathname", parent / name)
                ("name", name)
                ("mask", mask)
                ("cookie", event.cookie);
        }
    };
}


struct rask::notification::impl {
    f5::notifications<callback> notifications;

    impl(boost::asio::io_service &s)
    : notifications(s) {
    }
};


rask::notification::notification(boost::asio::io_service &s)
: pimpl(new impl(s)) {
}


rask::notification::~notification() {
}


void rask::notification::operator () (rask::workers &) {
    pimpl->notifications();
}


bool rask::notification::watch(std::shared_ptr<tenant> tenant, const boost::filesystem::path &folder) {
    bool watched = false;
    pimpl->notifications.watch(folder.c_str(),
        [this, &watched, tenant, &folder](int wd) {
            watched = true;
            g_watches.add(wd, std::make_pair(tenant, folder));
        },
        [](){});
    return watched;
}

