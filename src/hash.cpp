/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include "tree.hpp"
#include <rask/base32.hpp>
#include <rask/configuration.hpp>
#include <rask/subscriber.hpp>
#include <rask/tenant.hpp>

#include <f5/threading/set.hpp>

#include <beanbag/beanbag>
#include <fost/counter>
#include <fost/crypto>
#include <fost/insert>
#include <fost/log>


namespace {
    fostlib::performance p_inodes(rask::c_fost_rask, "hash", "inodes");
    fostlib::performance p_inodes_empty(rask::c_fost_rask, "hash", "inodes-empty");
    fostlib::performance p_inodes_exec(rask::c_fost_rask, "hash", "inodes-executed");
    fostlib::performance p_tenants(rask::c_fost_rask, "hash", "tenants");
}


rask::name_hash_type rask::name_hash(const fostlib::string &s) {
    fostlib::digester d(fostlib::md5);
    d << s;
    return fostlib::coerce<fostlib::string>(
        fostlib::coerce<rask::base32_string>(d.digest()));
}


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

    f5::tsset<beanbag::jsondb_ptr> g_inodes;
    f5::tsset<beanbag::jsondb_ptr> g_tenants;

    void rehash(rask::workers &w) {
        static const fostlib::jcursor inode_hash_path("hash", "inode");
        auto pdb = g_inodes.pop_back();
        if ( not pdb ) {
            ++p_inodes_empty;
            return;
        }
        ++p_inodes_exec;

        fostlib::jsondb::local tdb(*pdb);
        std::vector<unsigned char> hash(
            digest(inode_hash_path, tdb["inodes"]));
        if ( tdb.has_key("layer") ) {
            try {
                const auto tenant_name = fostlib::coerce<fostlib::string>(tdb["tenant"]);
                auto tenant = rask::known_tenant(w, tenant_name);
                if ( !tenant->subscription ) {
                    throw fostlib::exceptions::null("Rehash of tenant with no subscription");
                }
                auto pdbp = tenant->subscription->inodes().layer_dbp(
                    fostlib::coerce<std::size_t>(tdb["layer"]["index"]) - 1,
                    fostlib::coerce<fostlib::string>(tdb["layer"]["hash"]));
                fostlib::jsondb::local parent(*pdbp);
                // Set the correct hash in the parent's child node
                parent
                    .set(fostlib::jcursor("inodes") /
                            fostlib::coerce<fostlib::string>(tdb["layer"]["current"]) /
                            inode_hash_path,
                        fostlib::coerce<fostlib::base64_string>(hash))
                    .commit();
                // Then rehash the parent and on up
                rask::rehash_inodes(w, pdbp);
            } catch ( fostlib::exceptions::exception &e ) {
                fostlib::json level;
                fostlib::insert(level, "tdb", tdb.data());
                fostlib::push_back(e.data(), "rehash_inodes", level);
                throw;
            }
        } else {
            std::shared_ptr<rask::tenant> tenantp(
                rask::known_tenant(w, fostlib::coerce<fostlib::string>(tdb["tenant"])));
            beanbag::jsondb_ptr dbp(beanbag::database(rask::c_tenant_db.value()));
            fostlib::jsondb::local tenants(*dbp);
            tenants
                .set(fostlib::jcursor("known", tenantp->name(), "hash", "data"),
                    fostlib::coerce<fostlib::string>(
                        fostlib::coerce<fostlib::base64_string>(hash)))
                .commit();
            rask::rehash_tenants(dbp);
            std::array<unsigned char, 32> hash_array;
            std::copy(hash.begin(), hash.end(), hash_array.begin());
            tenantp->hash = hash_array;
        }
    }
}


void rask::rehash_inodes(workers &w, const fostlib::json &dbconfig) {
    auto pdb(beanbag::database(dbconfig));
    rehash_inodes(w, pdb);
}


void rask::rehash_inodes(workers &w, beanbag::jsondb_ptr pdb) {
    ++p_inodes;
    g_inodes.insert_if_not_found(pdb);
    w.high_latency.get_io_service().post(
        [&w]() {
            rehash(w);
        });
}


void rask::rehash_tenants(beanbag::jsondb_ptr pdb) {
    ++p_tenants;
    fostlib::jsondb::local tsdb(*pdb);
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

