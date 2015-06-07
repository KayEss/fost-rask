/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/tenant.hpp>

#include <fost/log>


rask::connection::out rask::move_out_packet(
    rask::tenant &tenant, const rask::tick &priority,
    const fostlib::string &name
) {
    connection::out packet(0x93);
    packet << priority << tenant.name() << name;
    return std::move(packet);
}


void rask::move_out(rask::connection::in &packet) {
    auto logger(fostlib::log::info());
    logger("", "Remove inode");
    auto priority(packet.read<tick>());
    logger("priority", priority);
    auto tenant(known_tenant(packet.read<fostlib::string>()));
    auto name(packet.read<fostlib::string>());
    logger
        ("tenant", tenant->name())
        ("name", name);
    packet.socket->workers.high_latency.io_service.post(
        [tenant, name = std::move(name), priority]() {
            auto location = tenant->local_path() /
                fostlib::coerce<boost::filesystem::path>(name);
            tenant->remote_change(location, tenant::move_inode_out, priority);
            auto removed = boost::filesystem::remove_all(location);
            if ( removed ) {
                fostlib::log::warning()
                    ("", "Deleting files")
                    ("tenant", tenant->name())
                    ("root", location)
                    ("count", removed);
            }
        });
}

