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


    /// Store information we've heard about a peer
    class peer {
    public:
        /// Return the peer corresponding to the identifier given
        static peer &server(uint32_t);

        /// Store information about the server on the other end
        std::atomic<std::array<unsigned char, 32>> hash;

        /// The view of a tenant that the other server has
        class tenant;

        /// The tenants
        f5::tsmap<fostlib::string, std::unique_ptr<tenant>> tenants;
    };


}



class rask::peer::tenant {
public:
    /// The other server's hash for this tenant
    std::atomic<std::array<unsigned char, 32>> hash;

    class inode;

    /// The inodes under discussion
    f5::tsmap<fostlib::string, inode> inodes;
};


class rask::peer::tenant::inode {
};

