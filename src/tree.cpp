/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include "tree.hpp"
#include <rask/base32.hpp>
#include <rask/workers.hpp>

#include <fost/log>


namespace {
    const fostlib::json c_db_cluster("db-cluster");
}


/*
    rask::tree
*/


rask::tree::tree(
    rask::workers &w, fostlib::json c, fostlib::jcursor r, fostlib::jcursor nh, fostlib::jcursor ih
) : workers(w), root_db_config(std::move(c)), root(std::move(r)),
        name_hash_path(std::move(nh)), item_hash_path(std::move(ih)) {
}


beanbag::jsondb_ptr rask::tree::root_dbp() const {
    return beanbag::database(root_db_config);
}


rask::tree::const_iterator rask::tree::begin() const {
    const_iterator iter(*this);
    iter.begin(root_dbp());
    return iter;
}


rask::tree::const_iterator rask::tree::end() const {
    const_iterator iter(*this);
    iter.end();
    return iter;
}


namespace {
    fostlib::jsondb::local add_leaf(rask::workers &workers,
        std::size_t layer, const fostlib::json &dbconfig, fostlib::jsondb::local meta,
        const rask::tree &tree, const fostlib::jcursor &dbpath
    ) {
        if ( !meta.has_key(dbpath) ) {
            meta.insert(dbpath, fostlib::json::object_t());
        }
        meta.pre_commit(
            [&workers, layer, &dbconfig, &tree](fostlib::json &data) {
                if ( data[tree.key()].size() > 64 ) {
                    fostlib::json items(data[tree.key()]);
                    tree.key().replace(data, fostlib::json::object_t());
                    fostlib::insert(data, "@context", c_db_cluster);
                    for ( auto niter(items.begin()); niter != items.end(); ++niter ) {
                        auto item = *niter;
                        auto hash = fostlib::coerce<fostlib::string>(item[tree.name_hash_path()]);
                        if ( layer >= hash.length() )
                            throw fostlib::exceptions::not_implemented(
                                "Partitioning a tree when we've run out of name hash");
                        auto hkey = fostlib::string(1, hash[layer]);
                        beanbag::jsondb_ptr hdbp;
                        if ( data[tree.key()].has_key(hkey) ) {
                            hdbp = beanbag::database(data[tree.key()][hkey]["database"]);
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
                            (tree.key() / hkey / "database").insert(data, conf);
                            hdbp = beanbag::database(conf);
                            fostlib::jsondb::local child(*hdbp);
                            child
                                .insert("parent", dbconfig)
                                .insert("child", hkey)
                                .insert(tree.key(), fostlib::json::object_t())
                                .commit();
                        }
                        fostlib::jsondb::local child(*hdbp);
                        child
                            .insert(tree.key() / niter.key(), item)
                            .commit();
                    }
                    for ( auto db : data[tree.key()] ) {
                        workers.high_latency.io_service.post(
                            [db]() {
                                rask::rehash_inodes(db["database"]);
                            });
                    }
                }
            });
        return std::move(meta);
    }
    fostlib::jsondb::local add_recurse(rask::workers &workers,
        std::size_t layer, const fostlib::json &dbconfig, fostlib::jsondb::local meta,
        const rask::tree &tree, const fostlib::jcursor &dbpath, const fostlib::string &hash
    ) {
        if ( !meta.data().has_key("@context") ) {
            return add_leaf(workers, layer, dbconfig, std::move(meta), tree, dbpath);
        } else {
            fostlib::json dbconf(meta[tree.key()][fostlib::string(1, hash[layer])]["database"]);
            beanbag::jsondb_ptr pdb(beanbag::database(dbconf));
            return add_recurse(workers, layer + 1, dbconf, fostlib::jsondb::local(*pdb),
                tree, dbpath, hash);
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
    return add_recurse(workers, 0, root_db_config, std::move(meta), *this, dbpath, hash);
}


/*
    rask::tree::const_iterator
*/


rask::tree::const_iterator::const_iterator(const rask::tree &t)
: tree(t) {
    layers.reserve(10); // This is more than enough to start with (2x10^14)
}


rask::tree::const_iterator::const_iterator(const_iterator &&iter)
: tree(iter.tree), layers(std::move(iter.layers)) {
}


bool rask::tree::const_iterator::check_pop() {
    if ( layers.size() ) {
        if ( layers.rbegin()->pos == layers.rbegin()->end ) {
            layers.pop_back();
            return true;
        }
    }
    return false;
}


void rask::tree::const_iterator::begin(beanbag::jsondb_ptr dbp) {
    fostlib::jsondb::local layer(*dbp);
    const bool bottom(!layer.data().has_key("@context"));
    layers.emplace_back(dbp, std::move(layer), layer[tree.key()]);
    if ( !bottom ) {
        begin(beanbag::database((*layers.rbegin()->pos)["database"]));
    } else {
        check_pop();
    }
}


/// We don't need this to do something if the iterator advancement
/// and begin can leave the layer empty when they get to the end
void rask::tree::const_iterator::end() {
}


fostlib::json rask::tree::const_iterator::operator * () const {
    if ( layers.size() ) {
        return *(layers.rbegin()->pos);
    } else {
        throw fostlib::exceptions::not_implemented(
            "rask::tree::const_iterator::operator * when layers is empty");
    }
}


rask::tree::const_iterator &rask::tree::const_iterator::operator ++ () {
    ++(layers.rbegin()->pos);
    while ( check_pop() ) {
        if ( layers.size() ) {
            ++(layers.rbegin()->pos);
        }
    }
    if ( layers.size() && layers.rbegin()->meta.has_key("@context") ) {
        begin(beanbag::database((*layers.rbegin()->pos)["database"]));
    }
    return *this;
}


bool rask::tree::const_iterator::operator == (const rask::tree::const_iterator &r) const {
    return layers == r.layers;
}

