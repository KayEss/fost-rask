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
    struct callback : public f5::boost_asio::reader {
        std::map<int, boost::filesystem::path> &directories;

        callback(std::map<int, boost::filesystem::path> &d, boost::asio::io_service &s)
        : reader(s), directories(d) {
        }

        void process(const inotify_event &event) {
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
                ("wd", "directory", directories[event.wd])
                ("wd", "pathname", directories[event.wd] / name)
                ("name", name)
                ("mask", mask)
                ("cookie", event.cookie);
        }
    };
}


struct rask::notification::impl {
    f5::notifications<callback> notifications;
    std::map<int, boost::filesystem::path> directories;

    impl(boost::asio::io_service &s)
    : notifications(directories, s) {
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


bool rask::notification::watch(const boost::filesystem::path &folder) {
    bool watched = false;
    pimpl->notifications.watch(folder.c_str(),
        [this, &watched, &folder](int wd) {
            watched = true;
            pimpl->directories[wd] = folder;
        },
        [](){});
    return watched;
}

