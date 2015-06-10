/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "tree.hpp"
#include <rask/base32.hpp>

#include <fost/log>


namespace {
    const fostlib::json c_db_cluster("db-cluster");
}


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


namespace {
    fostlib::jsondb::local add_leaf(
        std::size_t layer, const fostlib::json &dbconfig, fostlib::jsondb::local meta,
        const rask::tree &tree, const fostlib::jcursor &dbpath
    ) {
        if ( !meta.has_key(dbpath) ) {
            meta.insert(dbpath, fostlib::json::object_t());
        }
        meta.pre_commit(
            [layer, &dbconfig, &tree](fostlib::json &data) {
                if ( data[tree.key()].size() > 64 ) {
                    fostlib::json items(data[tree.key()]);
                    tree.key().replace(data, fostlib::json::object_t());
                    fostlib::insert(data, tree.key(), "@context", c_db_cluster);
                    for ( auto niter(items.begin()); niter != items.end(); ++niter ) {
                        auto item = *niter;
                        auto hash = fostlib::coerce<fostlib::string>(item[tree.name_hash_path()]);
                        if ( layer >= hash.length() )
                            throw fostlib::exceptions::not_implemented(
                                "Partitioning a tree when we've run out of name hash");
                        auto hkey = fostlib::string(1, hash[layer]);
                        beanbag::jsondb_ptr hdbp;
                        if ( data[tree.key()].has_key(hkey) ) {
                            hdbp = beanbag::database(data[tree.key()][hkey]);
                        } else {
                            auto ndb_path(fostlib::coerce<boost::filesystem::path>(
                                dbconfig["filepath"]));
                            ndb_path.replace_extension(
                                fostlib::coerce<boost::filesystem::path>(hkey + ".json"));
                            fostlib::json conf;
                            fostlib::insert(conf, "filepath", ndb_path);
                            if ( layer == 0u ) {
                                fostlib::insert(conf, "name",
                                    fostlib::coerce<fostlib::string>(dbconfig["name"]) + "/" + hkey);
                            } else {
                                fostlib::insert(conf, "name",
                                    fostlib::coerce<fostlib::string>(dbconfig["name"]) + hkey);
                            }
                            fostlib::insert(conf, "initial", fostlib::json::object_t());
                            (tree.key() / hkey).insert(data, conf);
                            hdbp = beanbag::database(conf);
                            fostlib::jsondb::local child(*hdbp);
                            child
                                .insert("parent", dbconfig)
                                .insert(tree.key(), fostlib::json::object_t())
                                .commit();
                        }
                        fostlib::jsondb::local child(*hdbp);
                        child
                            .insert(tree.key() / niter.key(), item)
                            .commit();
                    }
                }
            });
        return std::move(meta);
    }
    fostlib::jsondb::local add_recurse(
        std::size_t layer, const fostlib::json &dbconfig, fostlib::jsondb::local meta,
        const rask::tree &tree, const fostlib::jcursor &dbpath, const fostlib::string &hash
    ) {
        if ( !meta[tree.key()].has_key("@context") ) {
            return add_leaf(layer, dbconfig, std::move(meta), tree, dbpath);
        } else {
            throw fostlib::exceptions::not_implemented(
                "recursing into a sub-database");
        }
    }
}
fostlib::jsondb::local rask::tree::add(
    const fostlib::jcursor &dbpath,
    const fostlib::string &path, const std::vector<unsigned char> &hashv
) {
    auto dbp = root_dbp();
    fostlib::jsondb::local meta(*dbp);
    auto hash = fostlib::coerce<fostlib::string>(
        fostlib::coerce<rask::base32_string>(hashv));
    return add_recurse(0, root_db_config, std::move(meta), *this, dbpath, hash);
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


