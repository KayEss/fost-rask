/*
    Copyright 2015-2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash.hpp"
#include "tree.hpp"
#include <rask/base32.hpp>
#include <rask/configuration.hpp>
#include <rask/workers.hpp>

#include <fost/counter>
#include <fost/log>


namespace {
    fostlib::performance p_deferred(rask::c_fost_rask,
        "tree", "partitioned-during-commit");
}


const fostlib::json rask::c_db_cluster("db-cluster");


/*
    rask::tree
*/


rask::tree::tree(
    rask::workers &w, fostlib::json c, fostlib::jcursor r,
    fostlib::jcursor nh, fostlib::jcursor ih
) : workers(w), root_db_config(std::move(c)), root(std::move(r)),
        name_hash_path(std::move(nh)), item_hash_path(std::move(ih)),
        hash(*this) {
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
    void do_lookup(
        rask::tree &t,
        std::size_t layer,
        const rask::name_hash_type &hash,
        const fostlib::string &location,
        std::function<void(const fostlib::json &)> found
    ) {
        auto dbp = t.layer_dbp(layer, hash);
        fostlib::jsondb::local data(*dbp);
        const bool recurse = rask::partitioned(data);
        if ( recurse ) {
            do_lookup(t, layer + 1u, hash, location, found);
        } else {
            if ( data["inodes"].has_key(location) )
                found(data["inodes"][location]);
        }
    }
}
void rask::tree::lookup(
    const name_hash_type &hash,
    const boost::filesystem::path &location,
    std::function<void(const fostlib::json &)> found
) {
    do_lookup(*this, 0u, hash, fostlib::coerce<fostlib::string>(location), found);
}


namespace {
    void add_recurse(rask::workers &workers,
        std::size_t layer, fostlib::jsondb::local meta,
        const rask::tree &tree, const fostlib::jcursor &dbpath,
        const rask::name_hash_type &hash,
        rask::tree::manipulator_fn manipulator,
        std::function<void(void)> post);

    void partition_if_needed(
        rask::workers &workers,
        std::size_t layer,
        const rask::tree &tree,
        fostlib::json &data
    ) {
        if ( data[tree.key()].size() > 96 ) {
            fostlib::log::debug(rask::c_fost_rask)
                ("", "partitioning beanbag")
                ("layer", data["layer"]);
            std::vector<beanbag::jsondb_ptr> children(32);
            fostlib::json items(data[tree.key()]);
            tree.key().replace(data, fostlib::json::object_t());
            fostlib::insert(data, "@context", rask::c_db_cluster);
            for ( auto niter(items.begin()); niter != items.end(); ++niter ) {
                auto item = *niter;
                const auto hash = fostlib::coerce<fostlib::string>(
                    item[tree.name_hash_path()]);
                if ( layer >= hash.length() )
                    throw fostlib::exceptions::not_implemented(
                        "Partitioning a tree when we've run out of name hash");
                const auto digit = rask::from_base32_ascii_digit(hash[layer]);
                if ( !children[digit] ) {
                    fostlib::insert(data, tree.key(), hash.substr(layer, 1), fostlib::json::object_t());
                    // TODO When we create the child database there
                    // may (through some earlier error) be a JSON file
                    // already in existence. We don't want that as
                    // we're partiioning into new databases. We need
                    // to ensure that broken JSON files get wiped.
                    // Passing true to layer_dbp does that.
                    children[digit] = tree.layer_dbp(layer + 1, hash, true);
                }
                fostlib::jsondb::local child(*children[digit]);
                child
                    .insert(tree.key() / niter.key(), item)
                    .commit();
            }
            for ( auto dbp : children ) {
                if ( dbp ) {
                    workers.hashes.get_io_service().post(
                        [&workers, dbp]() {
                            rask::rehash_inodes(workers, dbp);
                        });
                }
            }
        }
    }

    void add_leaf(rask::workers &workers,
        std::size_t layer, fostlib::jsondb::local meta,
        const rask::tree &tree, const fostlib::jcursor &dbpath,
        const rask::name_hash_type &hash,
        rask::tree::manipulator_fn manipulator,
        std::function<void(void)> post
    ) {
        bool deferred = false;
        meta.transformation(
            [&workers, &tree, manipulator, post, dbpath, layer, hash, &deferred](
                const fostlib::jcursor &root, fostlib::json &data
            ) {
                assert(root.size() == 0u);
                const bool recurse = rask::partitioned(data);
                try {
                    if ( recurse ) {
                        ++p_deferred;
                        deferred = true;
                        /// If we find that we need to recurse down, then we are almost
                        /// certainly recursing up at the same time due to child inode
                        /// rehashing. Therefore we'll allow this commit to complete
                        /// and recurse down in a new job so we aren't holding the lock
                        /// for any longer than we need to.
                        workers.files.get_io_service().post(
                            [&workers, &tree, manipulator, post, dbpath, layer, hash]() {
                                beanbag::jsondb_ptr pdb(tree.layer_dbp(layer + 1, hash));
                                add_recurse(workers, layer + 1, fostlib::jsondb::local(*pdb),
                                    tree, dbpath, hash, manipulator, post);
                            });
                    } else {
                        if ( !data.has_key(dbpath) ) {
                            dbpath.insert(data, fostlib::json::object_t());
                        }
                        manipulator(workers, data, tree.layer_db_config(layer, hash));
                        partition_if_needed(workers, layer, tree, data);
                    }
                } catch ( fostlib::exceptions::exception &e ) {
                    fostlib::push_back(e.data(), "add", "stack", "meta.transformation");
                    fostlib::push_back(e.data(), "add", "layer", int64_t(layer));
                    fostlib::push_back(e.data(), "add", "hash", hash);
                    fostlib::push_back(e.data(), "add", "recurse", recurse);
                    fostlib::push_back(e.data(), "add", "data", data);
                    throw;
                }
            });
        meta.commit();
        if ( !deferred ) post();
    }
    void add_recurse(rask::workers &workers,
        std::size_t layer, fostlib::jsondb::local meta,
        const rask::tree &tree, const fostlib::jcursor &dbpath,
        const rask::name_hash_type &hash,
        rask::tree::manipulator_fn manipulator,
        std::function<void(void)> post
    ) {
        const bool recurse = rask::partitioned(meta);
        if ( recurse ) {
            try {
                beanbag::jsondb_ptr pdb(tree.layer_dbp(layer + 1, hash));
                add_recurse(workers, layer + 1, fostlib::jsondb::local(*pdb),
                    tree, dbpath, hash, manipulator, post);
            } catch ( fostlib::exceptions::exception &e ) {
                fostlib::push_back(e.data(), "add", "stack", "add_recurse");
                fostlib::push_back(e.data(), "add", "layer", int64_t(layer));
                fostlib::push_back(e.data(), "add", "hash", hash);
                fostlib::push_back(e.data(), "add", "recurse", recurse);
                fostlib::push_back(e.data(), "add", "data", meta.data());
                throw;
            }
        } else {
            fostlib::json mdata(meta.data());
            try {
                add_leaf(workers, layer, std::move(meta),
                    tree, dbpath, hash, manipulator, post);
            } catch ( fostlib::exceptions::exception &e ) {
                fostlib::push_back(e.data(), "add", "stack", "add_leaf");
                fostlib::push_back(e.data(), "add", "layer", int64_t(layer));
                fostlib::push_back(e.data(), "add", "hash", hash);
                fostlib::push_back(e.data(), "add", "recurse", recurse);
                fostlib::push_back(e.data(), "add", "data", mdata);
                throw;
            }
        }
    }
}
void rask::tree::add(
    const fostlib::jcursor &dbpath,
    const fostlib::string &path, const name_hash_type &hash,
    rask::tree::manipulator_fn m, std::function<void(void)> post
) {
    auto dbp = root_dbp();
    fostlib::jsondb::local meta(*dbp);
    add_recurse(workers, 0,  std::move(meta), *this, dbpath, hash, m, post);
}


boost::filesystem::path rask::tree::dbpath(const boost::filesystem::path &sub) const {
    auto path_str = fostlib::coerce<fostlib::string>(root_db_config["filepath"]);
    path_str = path_str.substr(0, path_str.length() - 5); // strip .json
    return fostlib::coerce<boost::filesystem::path>(path_str) / sub;
}


fostlib::json rask::tree::layer_db_config(
    std::size_t layer, const name_hash_type &hash, bool wipe
) const {
    if ( layer == 0u ) {
        return root_db_config;
    } else {
        const auto hash_prefix = hash.substr(0, layer);
        const auto ndb_path = dbpath(fostlib::coerce<boost::filesystem::path>(
            rask::name_hash_path(hash.substr(0, layer)) + ".json"));
        if ( wipe and boost::filesystem::exists(ndb_path) ) {
            fostlib::log::warning(c_fost_rask)
                ("", "layer_db_config -- old beanbag file exists, deleting")
                ("layer", layer)
                ("hash", hash)
                ("path", ndb_path);
            boost::filesystem::remove(ndb_path);
        }
        fostlib::json conf;
        fostlib::insert(conf, "filepath", ndb_path);
        fostlib::insert(conf, "name",
            fostlib::coerce<fostlib::string>(root_db_config["name"]) + "/" + hash_prefix);
        fostlib::insert(conf, "initial", root_db_config["initial"]);
        fostlib::insert(conf, "initial", "layer", "index", layer);
        fostlib::insert(conf, "initial", "layer", "hash", hash_prefix);
        fostlib::insert(conf, "initial", "layer", "current", hash.substr(layer - 1, 1));
        fostlib::insert(conf, "initial", key(), fostlib::json::object_t());
        return conf;
    }
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


void rask::tree::const_iterator::down() {
    if ( layers.rbegin()->meta.has_key("layer") ) {
        const auto index = fostlib::coerce<int>(layers.rbegin()->meta["layer"]["index"]);
        const auto hash =
            fostlib::coerce<fostlib::string>(layers.rbegin()->meta["layer"]["hash"]) +
            fostlib::coerce<fostlib::string>(layers.rbegin()->pos.key());
        begin(tree.layer_dbp(index + 1, hash));
    } else {
        const auto hash = fostlib::coerce<fostlib::string>(layers.rbegin()->pos.key());
        begin(tree.layer_dbp(1, hash));
    }
}


void rask::tree::const_iterator::begin(beanbag::jsondb_ptr dbp) {
    fostlib::jsondb::local layer(*dbp);
    const bool split(partitioned(layer));
    layers.emplace_back(dbp, std::move(layer), layer[tree.key()]);
    if ( split ) {
        down();
    } else {
        check_pop();
    }
}


/// We don't need this to do something if the iterator advancement
/// and begin can leave the layer empty when they get to the end
void rask::tree::const_iterator::end() {
}


fostlib::string rask::tree::const_iterator::key() const {
    if ( layers.size() ) {
        return fostlib::coerce<fostlib::string>(layers.rbegin()->pos.key());
    } else {
        throw fostlib::exceptions::not_implemented(
            "rask::tree::const_iterator::key when layers is empty");
    }
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
    if ( layers.size() && partitioned(layers.rbegin()->meta) ) {
        down();
    }
    return *this;
}


bool rask::tree::const_iterator::operator == (const rask::tree::const_iterator &r) const {
    return layers == r.layers;
}

