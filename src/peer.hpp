/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/clock.hpp>
#include <rask/connection.hpp>

#include <f5/threading/map.hpp>
#include <f5/threading/ring.hpp>


/**
    The peers are all about the data coming in from another from another
    node.
*/

namespace rask {


    struct workers;


    /// Kick off a peering arrangement
    void peer_with(workers &, std::shared_ptr<connection::reconnect>);

    /// Reset the watchdog timer
    void reset_watchdog(workers &w, std::shared_ptr<connection::reconnect>);


}
