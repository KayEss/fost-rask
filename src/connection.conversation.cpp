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
                auto ptenant = self->partner->tenants.find(name);
                if ( !ptenant ) {
                    tenant_packet(name, *iter)(self->socket);
                } else {
                    std::shared_ptr<tenant> mytenant(known_tenant(name));
                    if ( mytenant->hash.load() != ptenant->hash.load() ) {
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
    for ( auto iter(tenant->inodes().begin()); iter != tenant->inodes().end(); ++iter ) {
        const fostlib::json inode(*iter);
        auto &filetype = inode["filetype"];
        if ( filetype == tenant::directory_inode ) {
            fostlib::log::debug()
                ("", "sending create_directory")
                ("inode", inode);
            create_directory_out(*tenant, tick(inode["priority"]),
                    fostlib::coerce<fostlib::string>(inode["name"]))
                (self->socket);
        } else if ( filetype == tenant::move_inode_out ) {
            fostlib::log::debug()
                ("", "sending move_out")
                ("inode", inode);
            move_out_packet(*tenant, tick(inode["priority"]),
                    fostlib::coerce<fostlib::string>(inode["name"]))
                (self->socket);
        } else {
            fostlib::log::error()
                ("", "Unkown inode type to send to peer")
                ("inode", inode);
        }
    }
}

