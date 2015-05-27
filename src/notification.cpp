/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/workers.hpp>

#include <f5/fsnotify.hpp>
#include <f5/fsnotify/boost-asio.hpp>


struct rask::notification::impl {
    f5::notifications<f5::boost_asio::reader> notifications;
};


rask::notification::notification()
: pimpl(new impl) {
}


rask::notification::~notification() {
}


void rask::notification::operator () (rask::workers &) {
    pimpl->notifications();
}

