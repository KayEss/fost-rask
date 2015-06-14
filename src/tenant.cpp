/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "sweep.tenant.hpp"
#include "hash.hpp"
#include "tree.hpp"
#include <rask/base32.hpp>
#include <rask/clock.hpp>
#include <rask/configuration.hpp>
#include <rask/tenant.hpp>

#include <beanbag/beanbag>
#include <f5/threading/map.hpp>
#include <fost/crypto>
#include <fost/log>


namespace {
    f5::tsmap<fostlib::string, std::shared_ptr<rask::tenant>> g_tenants;
}


void rask::tenants(workers &w, const fostlib::json &dbconfig) {
    fostlib::log::debug("Loading tenants database", dbconfig);
    beanbag::jsondb_ptr dbp(beanbag::database(dbconfig));
    auto configure = [&w, dbp](const fostlib::json &tenants) {
        if ( tenants.has_key("subscription") ) {
            const fostlib::json subscriptions(tenants["subscription"]);
            for ( auto t(subscriptions.begin()); t != subscriptions.end(); ++t ) {
                auto key(fostlib::coerce<fostlib::string>(t.key()));
                g_tenants.add_if_not_found(key,
                    [&]() {
                        fostlib::log::info("New tenant for processing", t.key(), *t);
                        auto tp = std::make_shared<tenant>(w, key, *t);
                        start_sweep(w, tp);
                        return tp;
                    });
            }
        }
    };
    dbp->post_commit(configure);
    fostlib::jsondb::local db(*dbp);
    configure(db.data());
}


std::shared_ptr<rask::tenant> rask::known_tenant(const fostlib::string &n) {
    const std::shared_ptr<tenant> pt(g_tenants.find(n));
    if ( pt ) {
        return pt;
    } else {
        throw fostlib::exceptions::not_implemented(
            "rask::known_tenant for unknown tenant");
    }
}


/*
    rask::tenant
*/


const fostlib::json rask::tenant::directory_inode("directory");
const fostlib::json rask::tenant::move_inode_out("move-out");


namespace {
    auto slash(const fostlib::json &c) {
        if ( c.has_key("path") ) {
            auto root = fostlib::coerce<fostlib::string>(c["path"]);
            if ( !root.endswith('/') )
                root += '/';
            return std::move(root);
        } else {
            throw fostlib::exceptions::not_implemented(
                "Tenant must have a 'path' specifying the directory location");
        }
    }
}
rask::tenant::tenant(workers &w, const fostlib::string &n, const fostlib::json &c)
: root(slash(c)), name(n), configuration(c),
        local_path(fostlib::coerce<boost::filesystem::path>(root)) {
     // Tests will use this without a tenant configured, so make sure to check
    if ( c_tenant_db.value() != fostlib::json() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_tenant_db.value()));
        fostlib::jsondb::local tenants(*dbp);
        fostlib::jcursor dbpath("known", name(), "database");
        if ( !tenants.has_key(dbpath) ) {
            fostlib::log::debug()
                ("", "No tenant database found")
                ("tenants", "db-configuration", c_tenant_db.value())
                ("name", name())
                ("configuration", configuration());
            auto tdb_path(fostlib::coerce<boost::filesystem::path>(c_tenant_db.value()["filepath"]));
            tdb_path.replace_extension(fostlib::coerce<boost::filesystem::path>(name() + ".json"));
            fostlib::json conf;
            fostlib::insert(conf, "filepath", tdb_path);
            fostlib::insert(conf, "name", "tenant/" + name());
            fostlib::insert(conf, "initial", "tenant", name());
            tenants.set(dbpath, conf).commit();
        }
        // Set up the inodes
        inodes_p = std::make_unique<tree>(
            w, tenants[dbpath], fostlib::jcursor("inodes"),
            fostlib::jcursor("hash", "name"), fostlib::jcursor("hash", "inode"));
    }
}

rask::tenant::~tenant() = default;


beanbag::jsondb_ptr rask::tenant::beanbag() const {
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
void rask::tenant::local_change(
    const boost::filesystem::path &location,
    const fostlib::json &inode_type,
    packet_builder builder
) {
    auto path = relative_path(root, location);
    auto path_hash = name_hash(path);
    fostlib::jcursor dbpath(inodes().key(), fostlib::coerce<fostlib::string>(location));
    inodes().add(dbpath, path, path_hash,
        [
            self = this, inode_type, builder, dbpath,
            path = std::move(path), path_hash = std::move(path_hash)
        ](
            workers &w, fostlib::json &data, const fostlib::json &dbconf
        ) {
            if ( data[dbpath]["filetype"] != inode_type ) {
                auto priority = tick::next();
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
                w.high_latency.io_service.post([dbconf](){rehash_inodes(dbconf);});
                fostlib::log::info()
                    ("", inode_type)
                    ("broadcast", "to", broadcast(builder(*self, priority, path)))
                    ("tenant", self->name())
                    ("path", "relative", path)
                    ("node", node);
            }
        });
}
void rask::tenant::remote_change(
    const boost::filesystem::path &location,
    const fostlib::json &inode_type,
    const tick &priority
) {
//     auto path = relative_path(root, location);
//     auto path_hash = name_hash(path);
//     fostlib::jcursor dbpath(inodes().key(), fostlib::coerce<fostlib::string>(location));
//     fostlib::jsondb::local meta(inodes().add(dbpath, path, path_hash));
//     if ( meta[dbpath / "filetype"] != inode_type || tick(meta[dbpath / "priority"]) < priority ) {
//         auto path = relative_path(root, location);
//         fostlib::digester hash(fostlib::sha256);
//         hash << priority;
//         meta
//             .update(dbpath, fostlib::json::object_t())
//             .set(dbpath / "filetype", inode_type)
//             .set(dbpath / "name", path)
//             .set(dbpath / "priority", priority)
//             .set(dbpath / "hash" / "name", path_hash)
//             .set(dbpath / "hash" / "inode",
//                 fostlib::coerce<fostlib::string>(
//                     fostlib::coerce<fostlib::base64_string>(hash.digest())))
//             .commit();
//         rehash_inodes(meta);
//         fostlib::log::info()
//             ("", inode_type)
//             ("tenant", name())
//             ("path", "location", location)
//             ("path", "relative", path)
//             ("meta", meta[dbpath]);
//     }
}

