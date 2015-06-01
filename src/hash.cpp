/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include <rask/configuration.hpp>

#include <beanbag/beanbag>
#include <fost/crypto>
#include <fost/insert>
#include <fost/log>


namespace {
    auto digest(const fostlib::jcursor &p, const fostlib::json &o) {
        fostlib::digester hasher(fostlib::sha256);
        for ( auto n : o ) {
            try {
                hasher << fostlib::coerce<fostlib::string>(n[p]);
            } catch ( fostlib::exceptions::exception &e ) {
                fostlib::insert(e.data(), "for", o);
                fostlib::insert(e.data(), "data", n);
                fostlib::insert(e.data(), "path", p);
                throw;
            }
        }
        return fostlib::coerce<fostlib::string>(
                fostlib::coerce<fostlib::base64_string>(hasher.digest()));
    }
}


void rask::rehash_inodes(const tenant &tenant, const fostlib::jsondb::local &tdb) {
    beanbag::jsondb_ptr dbp(beanbag::database(c_tenant_db.value()));
    fostlib::jsondb::local tenants(*dbp);
    tenants
        .set(fostlib::jcursor("known", tenant.name(), "hash", "data"),
            digest(fostlib::jcursor("hash", "inode"), tdb["inodes"]))
        .commit();
    rehash_tenants(tenants);
}


void rask::rehash_tenants(const fostlib::jsondb::local &tsdb) {
    beanbag::jsondb_ptr dbp(beanbag::database(
        c_server_db.value()["database"]));
    fostlib::jsondb::local server(*dbp);
    server
        .set(fostlib::jcursor("hash"),
            digest(fostlib::jcursor("hash", "data"), tsdb["known"]))
        .commit();
}

