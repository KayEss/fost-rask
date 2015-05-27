/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <beanbag/beanbag>
#include <fost/log>

#include "sweep.tenant.hpp"
#include <rask/tenants.hpp>


namespace {
    fostlib::threadsafe_store<std::shared_ptr<rask::tenant>> g_tenants;
}


void rask::tenants(workers &w, const fostlib::json &dbconfig) {
    fostlib::log::debug("Loading tenants database", dbconfig);
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

