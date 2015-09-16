/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "subscriber.hpp"
#include <rask/tenant.hpp>

#include <fost/log>


rask::connection::out rask::move_out_packet(
    rask::tenant &tenant, const rask::tick &priority,
    const fostlib::string &name, const fostlib::json &
) {
    connection::out packet(0x93);
    packet << priority << tenant.name() << name;
    return std::move(packet);
}


void rask::move_out(rask::connection::in &packet) {
    auto logger(fostlib::log::info(c_fost_rask));
    logger("", "Remove inode");
    auto priority(packet.read<tick>());
    logger("priority", priority);
    auto tenant(
        known_tenant(packet.socket->workers, packet.read<fostlib::string>()));
    auto name(packet.read<fostlib::string>());
    logger
        ("tenant", tenant->name())
        ("name", name);
    if ( tenant->subscription ) {
        packet.socket->workers.files.get_io_service().post(
            [tenant, name = std::move(name), priority]() {
                tenant->subscription->move_out(name)
                    .compare_priority(priority)
                    .post_update(
                        [](const auto &c) {
                            auto removed = boost::filesystem::remove_all(c.location);
                            if ( removed ) {
                                fostlib::log::warning(c_fost_rask)
                                    ("", "Deleting files")
                                    ("tenant", c.subscription.tenant.name())
                                    ("root", c.location)
                                    ("count", removed);
                            }
                        })
                    .execute();
            });
    }
}

