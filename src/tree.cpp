/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "tree.hpp"

#include <fost/log>


/*
    rask::tree
*/


rask::tree::tree(fostlib::json c, fostlib::jcursor r, fostlib::jcursor h)
: root_db_config(std::move(c)), root(std::move(r)), name_hash_path(std::move(h)) {
}


beanbag::jsondb_ptr rask::tree::root_dbp() const {
    return beanbag::database(root_db_config);
}


rask::tree::const_iterator rask::tree::begin() const {
    const_iterator iter(*this, root_dbp());
    iter.begin();
    return iter;
}


rask::tree::const_iterator rask::tree::end() const {
    const_iterator iter(*this, root_dbp());
    iter.end();
    return iter;
}


/*
    rask::tree::const_iterator
*/


rask::tree::const_iterator::const_iterator(const rask::tree &t, beanbag::jsondb_ptr dbp)
: tree(t), root_dbp(dbp), root_data(*dbp) {
}


rask::tree::const_iterator::const_iterator(const_iterator &&iter)
: tree(iter.tree), root_dbp(iter.root_dbp), root_data(std::move(iter.root_data)),
        underlying(std::move(iter.underlying)) {
}

void rask::tree::const_iterator::begin()  {
    underlying = root_data[tree.root].begin();
}


void rask::tree::const_iterator::end() {
    underlying = root_data[tree.root].end();
}


fostlib::json rask::tree::const_iterator::operator * () const {
    return *underlying;
}


rask::tree::const_iterator &rask::tree::const_iterator::operator ++ () {
    ++underlying;
    return *this;
}


bool rask::tree::const_iterator::operator == (const rask::tree::const_iterator &r) const {
    return underlying == r.underlying;
}


