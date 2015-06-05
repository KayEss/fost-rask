/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.conversation.hpp"
#include "peer.hpp"

#include <rask/configuration.hpp>
#include <rask/workers.hpp>


rask::connection::conversation::conversation(std::shared_ptr<connection> socket)
: socket(socket), partner(peer::server(socket->identity)) {
    socket->conversing = true;
    auto tdbconf = c_tenant_db.value();
    if ( !tdbconf.isnull() ) {
        tenants_dbp = beanbag::database(tdbconf);
    }
}
rask::connection::conversation::~conversation() {
    socket->conversing = false;
}


void rask::connection::conversation::tenants(std::shared_ptr<conversation> self) {
    if ( !self->tenants_dbp ) return;
    self->socket->workers.low_latency.io_service.post(
        [self]() {
            fostlib::jsondb::local tenants(*self->tenants_dbp);
            fostlib::jcursor pos("known");
            for ( auto iter(tenants[pos].begin()); iter != tenants[pos].end(); ++iter ) {
                auto tenant = fostlib::coerce<fostlib::string>(iter.key());
                auto ptenant = self->partner.tenants.find(tenant);
                if ( !ptenant ) {
                    tenant_name(tenant)(self->socket);
                }
            }
        });
}

