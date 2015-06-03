/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.conversation.hpp"

#include <fost/log>


rask::connection::conversation::conversation(std::shared_ptr<connection> s)
: wsocket(s) {
}


void rask::connection::conversation::tenants(std::shared_ptr<conversation> self) {
}

