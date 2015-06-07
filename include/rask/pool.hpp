/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <cstddef>
#include <memory>

#include <boost/asio/io_service.hpp>


namespace rask {


    /// A Boost.ASIO service worker pool
    class pool {
    public:
        /// The Boost ASIO service
        boost::asio::io_service io_service;

        /// Determine how many threads are to service requests
        explicit pool(std::size_t threads);
        /// Stop processing on all threads
        ~pool();

        /// Make non-copyable and non assignable
        pool(const pool &) = delete;
        pool &operator = (const pool &) = delete;

    private:
        struct impl;
        std::unique_ptr<impl> pimpl;
    };


}

