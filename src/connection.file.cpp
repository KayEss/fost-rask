/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/connection.hpp>
#include <rask/subscriber.hpp>
#include <rask/tenant.hpp>

#include <fost/counter>


namespace {
    fostlib::performance p_file_exists_received(
        rask::c_fost_rask, "packets", "file_exists", "received");
    fostlib::performance p_file_exists_written(
        rask::c_fost_rask, "packets", "file_exists", "written");

    const fostlib::jcursor jc_priority("priority");
}


rask::connection::out rask::file_exists_out(
    tenant &t, const tick &p, const fostlib::string &n, const fostlib::json &
) {
    ++p_file_exists_written;
    connection::out packet(0x90);
    packet << p << t.name() << n;
    return std::move(packet);
}


void rask::file_exists(rask::connection::in &packet) {
    ++p_file_exists_received;
    auto logger(fostlib::log::info(c_fost_rask));
    logger("", "File exists");
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
                auto location = tenant->subscription->local_path() /
                    fostlib::coerce<boost::filesystem::path>(name);
                tenant->subscription->remote_change(
                    location,
                    tenant::file_inode,
                    priority,
                    [](const rask::tick &, fostlib::json inode) {
                        jc_priority.del_key(inode);
                        return inode;
                    });
            });
    }
}

