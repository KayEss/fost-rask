/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.conversation.hpp"
#include "peer.hpp"

#include <rask/configuration.hpp>
#include <rask/tenant.hpp>
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
    self->socket->workers.high_latency.io_service.post(
        [self]() {
            fostlib::jsondb::local tenants(*self->tenants_dbp);
            fostlib::jcursor pos("known");
            for ( auto iter(tenants[pos].begin()); iter != tenants[pos].end(); ++iter ) {
                auto name = fostlib::coerce<fostlib::string>(iter.key());
                auto ptenant = self->partner.tenants.find(name);
                if ( !ptenant ) {
                    tenant_packet(name, *iter)(self->socket);
                } else {
                    std::shared_ptr<tenant> mytenant(known_tenant(name));
                    if ( mytenant->hash.load() != (*ptenant)->hash.load() ) {
                        self->socket->workers.high_latency.io_service.post(
                            [self, mytenant]() {
                                inodes(self, mytenant);
                            });
                    }
                }
            }
        });
}


void rask::connection::conversation::inodes(
    std::shared_ptr<conversation> self, std::shared_ptr<tenant> tenant
) {
    fostlib::log::warning("Need to send inodes");
    fostlib::jsondb::local inodes(*tenant->beanbag(), "inodes");
    for ( auto iter(inodes.data().begin()); iter != inodes.data().end(); ++iter ) {
        fostlib::log::debug()
            ("", "sending create_directory")
            ("inode", *iter);
        create_directory_out(*tenant, tick((*iter)["priority"]),
                fostlib::coerce<fostlib::string>((*iter)["name"]))
            (self->socket);
    }
}

