/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "subscriber.hpp"
#include <rask/tenant.hpp>

#include <fost/counter>


namespace {
    fostlib::performance p_received(rask::c_fost_rask,
        "packets", "create_directory", "received");
}


rask::connection::out rask::create_directory_out(
    rask::tenant &tenant, const rask::tick &priority,
    const fostlib::string &name, const fostlib::json &
) {
    connection::out packet(0x91);
    packet << priority << tenant.name() << name;
    return std::move(packet);
}


void rask::create_directory(rask::connection::in &packet) {
    ++p_received;
    auto logger(fostlib::log::info(c_fost_rask));
    logger("", "Create directory");
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
                tenant->subscription->directory(name)
                    .compare_priority(priority)
                    .post_update(
                        [](subscriber::change &c) {
                            boost::filesystem::create_directories(c.location());
                        })
                    .execute();
            });
    }
}

