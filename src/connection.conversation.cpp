/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"

#include <fost/log>


rask::connection::conversation::conversation(std::shared_ptr<connection> s)
: wsocket(s) {
}

