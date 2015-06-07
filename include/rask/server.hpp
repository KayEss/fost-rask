/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/core>


namespace rask {


    struct workers;


    /// Return the server identity
    uint32_t server_identity();

    /// Start the Rask server (if configured)
    void server(workers &);

    /// Listen on a specified socket configuration
    void listen(workers &, const fostlib::json &configuration);


}

