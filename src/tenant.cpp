/*
    Copyright 2015-2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "subscriber.hpp"
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


void rask::tenants(
    workers &w, const fostlib::json &tdbconfig, const fostlib::json &sdbconfig
) {
    fostlib::log::debug(c_fost_rask)
        ("", "Loading tenants database")
        ("config", "tenants", tdbconfig)
        ("config", "subscriptions", sdbconfig);
    beanbag::jsondb_ptr dbp(beanbag::database(sdbconfig));
    auto configure = [&w, dbp](const fostlib::jcursor &root, const fostlib::json &subscribers) {
        assert(root.size() == 0u);
        if ( subscribers.has_key("subscription") ) {
            const fostlib::json subscriptions(subscribers["subscription"]);
            for ( auto t(subscriptions.begin()); t != subscriptions.end(); ++t ) {
                auto key(fostlib::coerce<fostlib::string>(t.key()));
                g_tenants.add_if_not_found(key,
                    [&]() {
                        fostlib::log::info(c_fost_rask,
                            "New subscriber for processing", t.key(), *t);
                        auto tp = std::make_shared<tenant>(w, key, *t);
                        start_sweep(w, tp);
                        return tp;
                    });
            }
        }
    };
    dbp->post_commit(configure);
    fostlib::jsondb::local db(*dbp);
    configure(fostlib::jcursor(), db.data());
}


std::shared_ptr<rask::tenant> rask::known_tenant(
    workers &w, const fostlib::string &tenant_name
) {
    return g_tenants.add_if_not_found(tenant_name,
        [&]() {
            fostlib::log::info(c_fost_rask,
                "New tenant (without subscription) added to internal database",
                tenant_name);
            auto tp = std::make_shared<tenant>(w, tenant_name);
            return tp;
        });
}


/*
    rask::tenant
*/


const fostlib::json rask::tenant::directory_inode("directory");
const fostlib::json rask::tenant::file_inode("file");
const fostlib::json rask::tenant::move_inode_out("move-out");


namespace {
    auto sub(rask::workers &w, rask::tenant &t, const fostlib::json &c) {
        if ( c.has_key("path") ) {
            return std::make_shared<rask::subscriber>(
                w, t, fostlib::coerce<fostlib::string>(c["path"]));
        } else {
            return std::shared_ptr<rask::subscriber>();
        }
    }
}
rask::tenant::tenant(workers &w, const fostlib::string &n)
: name(n) {
}
rask::tenant::tenant(workers &w, const fostlib::string &n, const fostlib::json &c)
: name(n), configuration(c), subscription(sub(w, *this, c)) {
}

rask::tenant::~tenant() = default;


