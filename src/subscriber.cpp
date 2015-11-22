/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "file.hpp"
#include "subscriber.hpp"
#include "tree.hpp"
#include <rask/configuration.hpp>
#include <rask/connection.hpp>
#include <rask/tenant.hpp>


namespace {
    auto slash(fostlib::string root) {
        if ( !root.endswith('/') )
            root += '/';
        return std::move(root);
    }
}
rask::subscriber::subscriber(workers &w, rask::tenant &t, fostlib::string path)
: root(slash(path)), tenant(t),
        local_path(fostlib::coerce<boost::filesystem::path>(root))
{
     // Tests will use this without a tenant configured, so make sure to check
    if ( c_tenant_db.value() != fostlib::json() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_tenant_db.value()));
        fostlib::jsondb::local tenants(*dbp);
        fostlib::jcursor dbpath("known", tenant.name(), "database");
        if ( !tenants.has_key(dbpath) ) {
            fostlib::log::debug(c_fost_rask)
                ("", "No tenant database found")
                ("tenants", "db-configuration", c_tenant_db.value())
                ("name", tenant.name())
                ("configuration", tenant.configuration());
            auto tdb_path(fostlib::coerce<boost::filesystem::path>(c_tenant_db.value()["filepath"]));
            tdb_path.replace_extension(
                fostlib::coerce<boost::filesystem::path>(tenant.name() + ".json"));
            fostlib::json conf;
            fostlib::insert(conf, "filepath", tdb_path);
            fostlib::insert(conf, "name", "tenant/" + tenant.name());
            fostlib::insert(conf, "initial", "tenant", tenant.name());
            tenants.set(dbpath, conf).commit();
        }
        // Set up the inodes
        inodes_p = std::make_unique<tree>(
            w, tenants[dbpath], fostlib::jcursor("inodes"),
            fostlib::jcursor("hash", "name"), fostlib::jcursor("hash", "inode"));
    }
}


beanbag::jsondb_ptr rask::subscriber::beanbag() const {
    return inodes_p->root_dbp();
}


rask::subscriber::change rask::subscriber::directory(
    const fostlib::string &relpath
) {
    return change(*this, relpath,
        local_path() / fostlib::coerce<boost::filesystem::path>(relpath),
        tenant::directory_inode);
}
rask::subscriber::change rask::subscriber::directory(
    const boost::filesystem::path &path
) {
    return change(*this, relative_path(root, path), path, tenant::directory_inode);
}


rask::subscriber::change rask::subscriber::file(
    const fostlib::string &relpath
) {
    return change(*this, relpath,
        local_path() / fostlib::coerce<boost::filesystem::path>(relpath),
        tenant::file_inode);
}
rask::subscriber::change rask::subscriber::file(
    const boost::filesystem::path &path
) {
    return change(*this, relative_path(root, path), path, tenant::file_inode);
}


rask::subscriber::change rask::subscriber::move_out(
    const fostlib::string &relpath
) {
    return change(*this, relpath,
        local_path() / fostlib::coerce<boost::filesystem::path>(relpath),
        tenant::move_inode_out);
}


/*
    rask::subscriber::change
*/


struct rask::subscriber::change::impl {
    /// Store the fields used for the final staus reporting
    status result;
    /// The that determines if the db entry is up to date or not
    std::function<bool(const fostlib::json &)> pred;
    /// Add in the new priority for the new inode data if use_priority returns true
    std::function<bool(const fostlib::json &)> use_priority;
    std::function<tick(const fostlib::json &)> priority;
    /// The function that generates the inode data hash
    std::function<fostlib::json(const tick &, const fostlib::json &)> hasher;
    /// Function to run when the predicate is `true`
    std::function<fostlib::json
        (fostlib::json&, const fostlib::jcursor&, status &)>
            if_predicate;
    /// Enrich the JSON that is used for a database update
    std::function<fostlib::json(fostlib::json)> enrich_update;
    /// Allow the database node to be changed even if there is no update
    std::function<fostlib::json(fostlib::json)> enrich_otherwise;
    /// Broadcast packet builder in case the database was updated.
    std::function<std::size_t(rask::tenant &, const rask::tick &,
        const fostlib::string &, const fostlib::json &)> broadcast;
    /// Functions to be run in the case where the database was updated
    /// and the transaction committed.
    std::vector<std::function<void(const change::status &)>> post_update;
    /// Functions to be run after the transaction is committed if there was no
    /// database update
    std::vector<std::function<void(const change::status &)>> post_otherwise;
    /// Functions to be run after the transaction is committed
    std::vector<std::function<void(const change::status &)>> post_commit;

    /// The tenant relative path
    fostlib::string relpath;
    /// The hash for the file name
    rask::name_hash_type nhash;
    /// The target inode type
    const fostlib::json &inode_target;
    /// The path into the database
    fostlib::jcursor dbpath;

    impl(
        subscriber &s, const fostlib::string &rp,
         const boost::filesystem::path &p, const fostlib::json &t
    ) :
        result(s, p),
        pred([this](const fostlib::json &j) {
            return j["filetype"] != inode_target;
        }),
        use_priority([](const auto &) {
            return true;
        }),
        priority([](const auto &) {
            return tick::next();
        }),
        hasher([](const auto &priority, const auto &) {
            fostlib::digester hash(fostlib::sha256);
            hash << priority;
            return fostlib::coerce<fostlib::json>(
                fostlib::coerce<fostlib::base64_string>(hash.digest()));
        }),
        if_predicate([pimpl = this](auto &data, const auto &dbpath, auto&) {
            fostlib::json node;
            fostlib::insert(node, "filetype", pimpl->inode_target);
            fostlib::insert(node, "name", pimpl->relpath);
            fostlib::insert(node, "hash", "name", pimpl->nhash);
            if ( pimpl->use_priority(pimpl->result.old) ) {
                auto priority = pimpl->priority(pimpl->result.old);
                fostlib::insert(node, "priority", priority);
                fostlib::insert(node, "hash", "inode",
                    pimpl->hasher(priority, node));
                node = pimpl->enrich_update(node);
                pimpl->broadcast(
                    pimpl->result.subscription.tenant,
                    priority, pimpl->relpath, node);
            } else {
                node = pimpl->enrich_update(node);
            }
            dbpath.replace(data, node);
            return node;
        }),
        enrich_update([](auto j) {
            return j;
        }),
        enrich_otherwise([](auto j) {
            return j;
        }),
        broadcast([](auto &, const auto &, const auto &, const auto &) {
            return 0u;
        }),
        relpath(rp),
        nhash(name_hash(relpath)),
        inode_target(t),
        dbpath(s.inodes().key(), fostlib::coerce<fostlib::string>(result.location))
    {}
};


rask::subscriber::change::change(
    subscriber &s, const fostlib::string &rp, const boost::filesystem::path &p,
    const fostlib::json &t
) : pimpl(std::make_shared<impl>(s, rp, p, t)) {
}


rask::subscriber::change::~change() = default;


rask::subscriber::change &rask::subscriber::change::predicate(
    std::function<bool(const fostlib::json &)> p
) {
    auto old_pred = pimpl->pred;
    pimpl->pred = [old_pred, p](const fostlib::json &j) {
        return old_pred(j) || p(j);
    };
    return *this;
}


rask::subscriber::change &rask::subscriber::change::compare_priority(
    const tick &t
) {
    return predicate([t](const fostlib::json &j) {
        return j["priority"].isnull() || rask::tick(j["priority"]) < t;
    }).record_priority(t);
}


rask::subscriber::change &rask::subscriber::change::record_priority(
    fostlib::t_null
) {
    pimpl->use_priority = [](const auto &) { return false; };
    return *this;
}
rask::subscriber::change &rask::subscriber::change::record_priority(
    const tick &t
) {
    pimpl->use_priority = [](const auto &) { return true; };
    pimpl->priority = [t](const auto &) { return t; };
    return *this;
}
rask::subscriber::change &rask::subscriber::change::record_priority(
    std::function<tick(const fostlib::json &)> f
) {
    pimpl->use_priority = [](const auto &) { return true; };
    pimpl->priority = f;
    return *this;
}


rask::subscriber::change &rask::subscriber::change::hash(
    std::function<fostlib::json(const tick &, const fostlib::json &)> h
) {
    pimpl->hasher = h;
    return *this;
}


rask::subscriber::change &rask::subscriber::change::if_predicate(
    std::function<fostlib::json(fostlib::json&, const fostlib::jcursor&, status &)> fn
) {
    pimpl->if_predicate = fn;
    return *this;
}


rask::subscriber::change &rask::subscriber::change::enrich_update(
    std::function<fostlib::json(fostlib::json)> f
) {
    pimpl->enrich_update = f;
    return *this;
}
rask::subscriber::change &rask::subscriber::change::enrich_otherwise(
    std::function<fostlib::json(fostlib::json)> f
) {
    pimpl->enrich_otherwise = f;
    return *this;
}


rask::subscriber::change &rask::subscriber::change::broadcast(
    std::function<connection::out(rask::tenant &, const rask::tick &,
        const fostlib::string &, const fostlib::json &)> b
) {
    pimpl->broadcast = [b](
            rask::tenant &t, const rask::tick &p,
            const fostlib::string &i, const fostlib::json &j
        ) {
            return rask::broadcast(
                [b, &t, p, i, j]() {
                    return std::move(b(t, p, i, j));
                });
        };
    return *this;
}


rask::subscriber::change &rask::subscriber::change::post_update(
    std::function<void(const status &)> f
) {
    pimpl->post_update.push_back(f);
    return *this;
}
rask::subscriber::change &rask::subscriber::change::post_otherwise(
    std::function<void(const status &)> f
) {
    pimpl->post_otherwise.push_back(f);
    return *this;
}


rask::subscriber::change &rask::subscriber::change::post_commit(
    std::function<void(const status &)> f
) {
    pimpl->post_commit.push_back(f);
    return *this;
}


void rask::subscriber::change::cancel() {
}
void rask::subscriber::change::execute() {
    pimpl->result.subscription.inodes().add(
        pimpl->dbpath, pimpl->relpath, pimpl->nhash,
        [pimpl = this->pimpl](
            workers &w, fostlib::json &data, const fostlib::json &dbconf
        ) {
            auto logger(fostlib::log::debug(c_fost_rask));
            logger("", "rask::subscriber::change::execute()")
                ("tenant", pimpl->result.subscription.tenant.name())
                ("dbpath", pimpl->dbpath);
            const bool entry = data.has_key(pimpl->dbpath);
            if ( entry ) {
                pimpl->result.old = data[pimpl->dbpath];
                logger("node", "old", pimpl->result.old);
            }
            if ( !entry || pimpl->pred(pimpl->result.old) ) {
                pimpl->result.inode =
                    pimpl->if_predicate(data, pimpl->dbpath, pimpl->result);
                pimpl->result.updated = (pimpl->result.old != pimpl->result.inode);
                logger("updating", pimpl->result.updated);
                if ( pimpl->result.updated ) {
                    rehash_inodes(w, dbconf);
                    logger("node", "new", pimpl->result.inode);
                }
            } else {
                logger("updating", false);
                pimpl->result.updated = false;
                pimpl->result.inode = pimpl->enrich_otherwise(pimpl->result.old);
                if ( pimpl->result.inode != pimpl->result.old ) {
                    pimpl->dbpath.replace(data, pimpl->result.inode);
                    logger("node", "new", pimpl->result.inode);
                }
            }
        },
        [pimpl = this->pimpl]() {
            if ( pimpl->result.updated ) {
                for ( auto &fn : pimpl->post_update ) {
                    fn(pimpl->result);
                }
            } else {
                for ( auto &fn : pimpl->post_otherwise ) {
                    fn(pimpl->result);
                }
            }
            for ( auto &fn : pimpl->post_commit ) {
                fn(pimpl->result);
            }
        });
}


/*
    rask::subscriber::change::result
*/


rask::subscriber::change::status::status(
    subscriber &s, boost::filesystem::path p
) :
    subscription(s), location(std::move(p)), updated(false)
{}

