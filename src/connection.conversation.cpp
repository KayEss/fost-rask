/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.conversation.hpp"
#include "peer.hpp"
#include "tree.hpp"

#include <rask/configuration.hpp>
#include <rask/tenant.hpp>
#include <rask/workers.hpp>


rask::connection::conversation::conversation(std::shared_ptr<connection> socket)
: socket(socket), partner(peer::server(socket->identity)) {
    auto tdbconf = c_tenant_db.value();
    if ( !tdbconf.isnull() ) {
        tenants_dbp = beanbag::database(tdbconf);
    }
}
rask::connection::conversation::~conversation() = default;

