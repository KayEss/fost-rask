/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "tree.hpp"
#include <rask/configuration.hpp>
#include <rask/connection.hpp>
#include <rask/subscriber.hpp>
#include <rask/tenant.hpp>


namespace {
    auto slash(fostlib::string root) {
        if ( !root.endswith('/') )
            root += '/';
        return std::move(root);
    }
}
rask::subscriber::subscriber(workers &w, rask::tenant &t, fostlib::string path)
: root(slash(path)), tenant(t),
        local_path(fostlib::coerce<boost::filesystem::path>(root))
{
     // Tests will use this without a tenant configured, so make sure to check
    if ( c_tenant_db.value() != fostlib::json() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_tenant_db.value()));
        fostlib::jsondb::local tenants(*dbp);
        fostlib::jcursor dbpath("known", tenant.name(), "database");
        if ( !tenants.has_key(dbpath) ) {
            fostlib::log::debug(c_fost_rask)
                ("", "No tenant database found")
                ("tenants", "db-configuration", c_tenant_db.value())
                ("name", tenant.name())
                ("configuration", tenant.configuration());
            auto tdb_path(fostlib::coerce<boost::filesystem::path>(c_tenant_db.value()["filepath"]));
            tdb_path.replace_extension(
                fostlib::coerce<boost::filesystem::path>(tenant.name() + ".json"));
            fostlib::json conf;
            fostlib::insert(conf, "filepath", tdb_path);
            fostlib::insert(conf, "name", "tenant/" + tenant.name());
            fostlib::insert(conf, "initial", "tenant", tenant.name());
            tenants.set(dbpath, conf).commit();
        }
        // Set up the inodes
        inodes_p = std::make_unique<tree>(
            w, tenants[dbpath], fostlib::jcursor("inodes"),
            fostlib::jcursor("hash", "name"), fostlib::jcursor("hash", "inode"));
    }
}


beanbag::jsondb_ptr rask::subscriber::beanbag() const {
    return inodes_p->root_dbp();
}


namespace {
    fostlib::string relative_path(
        const fostlib::string &root, const boost::filesystem::path &location
    ) {
        auto path = fostlib::coerce<fostlib::string>(location);
        if ( path.startswith(root) ) {
            path = path.substr(root.length());
        } else {
            fostlib::exceptions::not_implemented error("Directory is not in tenant root");
            fostlib::insert(error.data(), "root", root);
            fostlib::insert(error.data(), "location", location);
            throw error;
        }
        return path;
    }
}
void rask::subscriber::local_change(
    const boost::filesystem::path &location,
    const fostlib::json &inode_type,
    packet_builder builder, inode_function inoder
) {
    auto path = relative_path(root, location);
    auto path_hash = name_hash(path);
    fostlib::jcursor dbpath(inodes().key(), fostlib::coerce<fostlib::string>(location));
    inodes().add(dbpath, path, path_hash,
        [
            self = this, inode_type, builder, inoder, dbpath,
            path = std::move(path), path_hash = std::move(path_hash)
        ](
            workers &w, fostlib::json &data, const fostlib::json &dbconf
        ) {
            if ( data[dbpath]["filetype"] != inode_type ) {
                auto priority = tick::next();
                fostlib::json node;
                fostlib::insert(node, "filetype", inode_type);
                fostlib::insert(node, "name", path);
                fostlib::insert(node, "priority", priority);
                fostlib::insert(node, "hash", "name", path_hash);
                dbpath.replace(data, inoder(priority, node));
                w.hashes.get_io_service().post(
                    [&w, dbconf](){
                        rehash_inodes(w, dbconf);
                    });
                auto sent = broadcast(builder(self->tenant, priority, path));
                fostlib::log::info(c_fost_rask)
                    ("", inode_type)
                    ("broadcast", "to", sent)
                    ("tenant", self->tenant.name())
                    ("path", "relative", path)
                    ("node", node);
            }
        });
}
void rask::subscriber::local_change(
    const boost::filesystem::path &location,
    const fostlib::json &inode_type,
    packet_builder builder
) {
    local_change(location, inode_type, builder,
        [](const rask::tick &priority, fostlib::json inode) {
            fostlib::digester hash(fostlib::sha256);
            hash << priority;
            fostlib::insert(inode, "hash", "inode",
                fostlib::coerce<fostlib::base64_string>(hash.digest()));
            return inode;
        });
}
void rask::subscriber::remote_change(
    const boost::filesystem::path &location,
    const fostlib::json &inode_type,
    const tick &priority
) {
    auto path = relative_path(root, location);
    auto path_hash = name_hash(path);
    fostlib::jcursor dbpath(inodes().key(), fostlib::coerce<fostlib::string>(location));
    inodes().add(dbpath, path, path_hash,
        [
            self = this, inode_type, priority, dbpath,
            path = std::move(path), path_hash = std::move(path_hash)
        ](
            workers &w, fostlib::json &data, const fostlib::json &dbconf
        ) {
            if ( data[dbpath]["filetype"] != inode_type ||
                    tick(data[dbpath]["priority"]) < priority ) {
                fostlib::digester hash(fostlib::sha256);
                hash << priority;
                fostlib::json node;
                fostlib::insert(node, "filetype", inode_type);
                fostlib::insert(node, "name", path);
                fostlib::insert(node, "priority", priority);
                fostlib::insert(node, "hash", "name", path_hash);
                fostlib::insert(node, "hash", "inode",
                    fostlib::coerce<fostlib::base64_string>(hash.digest()));
                dbpath.replace(data, node);
                w.hashes.get_io_service().post(
                    [&w, dbconf](){
                        rehash_inodes(w, dbconf);
                    });
                fostlib::log::info(c_fost_rask)
                    ("", inode_type)
                    ("tenant", self->tenant.name())
                    ("path", "relative", path)
                    ("node", node);
            }
        });
}

