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


void rask::rehash_inodes(const tenant &tenant, const fostlib::jsondb::local &tdb) {
    const fostlib::jcursor hpath("hash", "inode");
    fostlib::digester hasher(fostlib::sha256);
    for ( auto inode : tdb["inodes"] ) {
        hasher << fostlib::coerce<fostlib::string>(inode[hpath]);
    }
    beanbag::jsondb_ptr dbp(beanbag::database(c_tenant_db.value()));
    fostlib::jsondb::local tenants(*dbp);
    tenants
        .set(fostlib::jcursor("known", tenant.name(), "hash", "data"),
            fostlib::coerce<fostlib::string>(
                fostlib::coerce<fostlib::base64_string>(hasher.digest())))
        .commit();
}

