/*
    Copyright 2015-2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/notification.hpp>
#include <rask/wiring.hpp>


namespace rask {


    /// Stores the workers in a way that they can be passed around
    struct workers {
        /// Construct the pools
        workers();
        /// Don't allow copying
        workers(const workers &) = delete;
        workers &operator = (const workers &) = delete;

        /// Worker pool for IO related tasks (i.e. low latency to react)
        f5::boost_asio::reactor_pool io;
        /// Worker pool for calculating responses to packets
        f5::boost_asio::reactor_pool responses;
        /// Worker pool for handling file management tasks
        f5::boost_asio::reactor_pool files;
        /// Worker pool for longer running tasks
        f5::boost_asio::reactor_pool hashes;
        /// File system notification
        notification notify;

        /// The entry point for the FRP processing
        wiring::local_server server;
    };


}

