/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include <rask/configuration.hpp>
#include <rask/tenant.hpp>

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
        return hasher.digest();
    }
}


rask::name_hash_type rask::name_hash(const fostlib::string &s) {
    fostlib::digester d(fostlib::md5);
    d << s;
    return d.digest();
}


void rask::rehash_inodes(tenant &tenant, const fostlib::jsondb::local &tdb) {
    std::vector<unsigned char> hash(
        digest(fostlib::jcursor("hash", "inode"), tdb["inodes"]));
    beanbag::jsondb_ptr dbp(beanbag::database(c_tenant_db.value()));
    fostlib::jsondb::local tenants(*dbp);
    tenants
        .set(fostlib::jcursor("known", tenant.name(), "hash", "data"),
            fostlib::coerce<fostlib::string>(
                fostlib::coerce<fostlib::base64_string>(hash)))
        .commit();
    rehash_tenants(tenants);
    std::array<unsigned char, 32> hash_array;
    std::copy(hash.begin(), hash.end(), hash_array.begin());
    tenant.hash = hash_array;
}


void rask::rehash_tenants(const fostlib::jsondb::local &tsdb) {
    std::vector<unsigned char> hash(
        digest(fostlib::jcursor("hash", "data"), tsdb["known"]));
    beanbag::jsondb_ptr dbp(beanbag::database(
        c_server_db.value()["database"]));
    fostlib::jsondb::local server(*dbp);
    server
        .set(fostlib::jcursor("hash"),
            fostlib::coerce<fostlib::string>(
                fostlib::coerce<fostlib::base64_string>(hash)))
        .commit();
}

