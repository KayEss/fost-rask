/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash-block.hpp"
#include "tree.hpp"
#include <rask/configuration.hpp>

#include <beanbag/beanbag>
#include <fost/counter>


namespace {
    fostlib::performance p_loaded_leaf(rask::c_fost_rask, "hash-block", "leaf");
    fostlib::performance p_loaded_mid(rask::c_fost_rask, "hash-block", "mid");
}


/*
    rask::block
*/


rask::block::block(const tree &t, block *p, std::size_t d, name_hash_type px)
: current(state::loaded), depth(d), prefix(std::move(px)), parent(p), partof(t) {
}


/*
    rask::leaf_block
*/


rask::leaf_block::leaf_block(const tree &t, block *b, std::size_t d, name_hash_type px)
: block(t, b, d, std::move(px)) {
    beanbag::jsondb_ptr dbp(partof.layer_dbp(depth, prefix));
    fostlib::jsondb::local db(*dbp);
    ++p_loaded_leaf;
}


void rask::leaf_block::manipulate(
    const boost::filesystem::path &location,
    const name_hash_type &hash,
    manipulator_fn manipulator
) {
}


/*
    rask::mid_block
*/


rask::mid_block::mid_block(const tree &t, block *b, std::size_t d, name_hash_type px)
: block(t, b, d, std::move(px)) {
    beanbag::jsondb_ptr dbp(partof.layer_dbp(depth, prefix));
    fostlib::jsondb::local db(*dbp);
    ++p_loaded_mid;
}


void rask::mid_block::manipulate(
    const boost::filesystem::path &location,
    const name_hash_type &hash,
    manipulator_fn manipulator
) {
}


/*
    rask::root_block
*/


rask::root_block::root_block(const tree &t)
: block(t, nullptr, 0u, name_hash_type()) {
    beanbag::jsondb_ptr dbp(partof.root_dbp());
    fostlib::jsondb::local db(*dbp);
    if ( partitioned(db) ) {
        actual.reset(new mid_block(t, this, 0u, name_hash_type()));
    } else {
        actual.reset(new leaf_block(t, this, 0u, name_hash_type()));
    }
}

