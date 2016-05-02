/*
    Copyright 2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/clock.hpp>
#include <f5/tamarind/boost-asio-reactor.hpp>


namespace rask {


    namespace wiring {


        /// A hash value for a server
        struct server_hash {
            tick time;
            uint32_t server;
            fostlib::base64_string hash;
        };


        /// The local server
        class local_server {
            struct impl;
            std::unique_ptr<impl> pimpl;
            f5::boost_asio::bridge block;
        public:
            /// Construct the local server instance
            local_server(f5::boost_asio::reactor_pool &);
            /// Allow destruction
            ~local_server();

            f5::boost_asio::bridge::input<tick> time;
            f5::boost_asio::bridge::input<server_hash> peer;
        };


    }


}

