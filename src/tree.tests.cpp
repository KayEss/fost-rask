/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "tree.hpp"
#include <rask/configuration.hpp>
#include <rask/tenant.hpp>
#include <rask/workers.hpp>
#include <fost/log>
#include <fost/test>


using namespace fostlib;


FSL_TEST_SUITE(tree);


FSL_TEST_FUNCTION(sub_db_config) {
    fostlib::json tenants;
    fostlib::insert(tenants, "name", "tenants");
    fostlib::insert(tenants, "filepath", "/tmp/tenants.json");
    fostlib::insert(tenants, "initial", "known", "t1", "database", "name", "tenant/t1");
    fostlib::insert(tenants, "initial", "known", "t1", "database", "filepath", "/tmp/t1.json");
    fostlib::insert(tenants, "initial", "known", "t1", "database", "initial", "tenant", "t1");
    fostlib::insert(tenants, "initial", "known", "t1", "subscription", "path", "/tmp/t1");
    fostlib::setting<fostlib::json> tenants_setting(
        "tree.tests.cpp", rask::c_tenant_db, tenants);

    rask::workers w;
    rask::tenant t(w, "t1", tenants["initial"]["known"]["t1"]["subscription"]);

    FSL_CHECK_EQ(t.inodes().layer_db_config(0, "01234"),
        tenants["initial"]["known"]["t1"]["database"]);
    FSL_CHECK_EQ(t.local_path(), "/tmp/t1/");

    auto layer1 = t.inodes().layer_db_config(1, "01234");
    fostlib::log::debug(rask::c_fost_rask, "layer1", layer1);
    FSL_CHECK_EQ(layer1["filepath"], fostlib::json("/tmp/t1/0.json"));
    FSL_CHECK_EQ(layer1["initial"]["layer"]["current"], fostlib::json("0"));
    FSL_CHECK_EQ(layer1["initial"]["layer"]["hash"], fostlib::json("0"));

    auto layer2 = t.inodes().layer_db_config(2, "01234");
    fostlib::log::debug(rask::c_fost_rask, "layer2", layer2);
    FSL_CHECK_EQ(layer2["filepath"], fostlib::json("/tmp/t1/01.json"));
    FSL_CHECK_EQ(layer2["initial"]["layer"]["current"], fostlib::json("1"));
    FSL_CHECK_EQ(layer2["initial"]["layer"]["hash"], fostlib::json("01"));

    auto layer3 = t.inodes().layer_db_config(3, "01234");
    fostlib::log::debug(rask::c_fost_rask, "layer3", layer3);
    FSL_CHECK_EQ(layer3["filepath"], fostlib::json("/tmp/t1/01/2.json"));
    FSL_CHECK_EQ(layer3["initial"]["layer"]["current"], fostlib::json("2"));
    FSL_CHECK_EQ(layer3["initial"]["layer"]["hash"], fostlib::json("012"));
}

