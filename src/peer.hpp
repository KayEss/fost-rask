/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/internet>


namespace rask {


    /// A connection between two Rask servers
    struct connection {
        boost::asio::ip::tcp::socket cnx;
    };


}

