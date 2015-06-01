/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "connection.hpp"
#include "sweep.tenant.hpp"
#include "hash.hpp"
#include <rask/clock.hpp>
#include <rask/tenants.hpp>

#include <beanbag/beanbag>
#include <fost/crypto>
#include <fost/log>


namespace {
    fostlib::threadsafe_store<std::shared_ptr<rask::tenant>> g_tenants;
    fostlib::json g_tenantsdb_config;

    const fostlib::json directory_inode("directory");
}


void rask::tenants(workers &w, const fostlib::json &dbconfig) {
    fostlib::log::debug("Loading tenants database", dbconfig);
    g_tenantsdb_config = dbconfig;
    beanbag::jsondb_ptr dbp(beanbag::database(dbconfig));
    auto configure = [&w, dbp](const fostlib::json &tenants) {
        if ( tenants.has_key("subscription") ) {
            const fostlib::json subscriptions(tenants["subscription"]);
            for ( auto t(subscriptions.begin()); t != subscriptions.end(); ++t ) {
                auto key(fostlib::coerce<fostlib::string>(t.key()));
                if ( g_tenants.find(key).size() == 0 ) {
                    fostlib::log::info("New tenant for processing", t.key(), *t);
                    auto tp = std::make_shared<tenant>(key, *t);
                    g_tenants.add(key, tp);
                    start_sweep(w, tp);
                }
            }
        }
    };
    dbp->post_commit(configure);
    fostlib::jsondb::local db(*dbp);
    configure(db.data());
}


/*
    rask::tenant
*/


rask::tenant::tenant(const fostlib::string &n, const fostlib::json &c)
: name(n), configuration(c) {
}


beanbag::jsondb_ptr rask::tenant::beanbag() const {
    beanbag::jsondb_ptr dbp(beanbag::database(g_tenantsdb_config));
    fostlib::jsondb::local tenants(*dbp);
    fostlib::jcursor dbpath("known", name(), "database");
    if ( !tenants.has_key(dbpath) ) {
        fostlib::log::debug()
            ("", "No tenant database found")
            ("tenants", "db-configuration", g_tenantsdb_config)
            ("name", name())
            ("configuration", configuration());
        auto tdb_path(fostlib::coerce<boost::filesystem::path>(g_tenantsdb_config["filepath"]));
        tdb_path.replace_extension(fostlib::coerce<boost::filesystem::path>(name() + ".json"));
        fostlib::json conf;
        fostlib::insert(conf, "filepath", tdb_path);
        fostlib::insert(conf, "name", "tenant/" + name());
        fostlib::insert(conf, "initial", fostlib::json::object_t());
        tenants.set(dbpath, conf).commit();
    }
    return beanbag::database(tenants[dbpath]);
}


void rask::tenant::dir_stat(const boost::filesystem::path &location) {
    beanbag::jsondb_ptr dbp(beanbag());
    fostlib::jsondb::local meta(*dbp);
    fostlib::jcursor dbpath("inodes", fostlib::coerce<fostlib::string>(location));
    if ( !meta.has_key(dbpath) || meta[dbpath / "filetype"] != directory_inode ) {
        auto path = fostlib::coerce<fostlib::string>(location);
        auto root = fostlib::coerce<fostlib::string>(configuration()["path"]);
        auto priority = tick::next();
        if ( path.startswith(root) ) {
            path = path.substr(root.length());
        } else {
            fostlib::exceptions::not_implemented error("Directory is not in tenant root");
            fostlib::insert(error.data(), "root", root);
            fostlib::insert(error.data(), "location", location);
            throw error;
        }
        fostlib::digester hash(fostlib::sha256);
        hash << priority;
        meta
            .set(dbpath, fostlib::json::object_t())
            .set(dbpath / "filetype", directory_inode)
            .set(dbpath / "name", path)
            .set(dbpath / "prority", priority)
            .set(dbpath / "hash" / "name", fostlib::sha256(path))
            .set(dbpath / "hash" / "inode",
                fostlib::coerce<fostlib::string>(
                    fostlib::coerce<fostlib::base64_string>(hash.digest())))
            .commit();
        rehash_inodes(*this, meta);
        fostlib::log::info()
            ("", "New folder")
            ("broadcast", broadcast(create_directory(
                *this, priority, meta, dbpath, path)))
            ("tenant", name())
            ("path", "location", location)
            ("path", "relative", path)
            ("path", "hash", meta[dbpath / "hash" / "name"]);
    }
}

