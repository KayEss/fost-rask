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
                if ( n.has_key(p) )
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


void rask::rehash_inodes(const fostlib::jsondb::local &tdb) {
    const fostlib::jcursor inode_hash_path("hash", "inode");
    std::vector<unsigned char> hash(
        digest(inode_hash_path, tdb["inodes"]));
    if ( tdb.has_key("parent") ) {
        auto pdbp = beanbag::database(tdb["parent"]);
        fostlib::jsondb::local parent(*pdbp);
        try {
            // Set the correct hash in the parent's child node
            parent
                .set(fostlib::jcursor("inodes") /
                        fostlib::coerce<fostlib::string>(tdb["child"]) /
                        inode_hash_path,
                    fostlib::coerce<fostlib::base64_string>(hash))
                .commit();
            // Then rehash the parent and on up
            rehash_inodes(parent);
        } catch ( fostlib::exceptions::exception &e ) {
            fostlib::json level;
            fostlib::insert(level, "tdb", tdb.data());
            fostlib::insert(level, "parent", parent.data());
            fostlib::push_back(e.data(), "rehash_inodes", level);
            throw;
        }
    } else {
        std::shared_ptr<rask::tenant> tenantp(
            known_tenant(fostlib::coerce<fostlib::string>(tdb["tenant"])));
        beanbag::jsondb_ptr dbp(beanbag::database(c_tenant_db.value()));
        fostlib::jsondb::local tenants(*dbp);
        tenants
            .set(fostlib::jcursor("known", tenantp->name(), "hash", "data"),
                fostlib::coerce<fostlib::string>(
                    fostlib::coerce<fostlib::base64_string>(hash)))
            .commit();
        rehash_tenants(tenants);
        std::array<unsigned char, 32> hash_array;
        std::copy(hash.begin(), hash.end(), hash_array.begin());
        tenantp->hash = hash_array;
    }
}


void rask::rehash_inodes(const fostlib::json &dbconfig) {
    fostlib::log::debug("rehash_inodes", dbconfig);
    auto pdb(beanbag::database(dbconfig));
    rehash_inodes(fostlib::jsondb::local(*pdb));
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

