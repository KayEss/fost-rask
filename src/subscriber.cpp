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


void rask::subscriber::local_change(
    const boost::filesystem::path &location,
    const fostlib::json &inode_type,
    condition_function pred, packet_builder builder, inode_function inoder,
    otherwise_function otherwise
) {
    auto path = relative_path(root, location);
    auto path_hash = name_hash(path);
    fostlib::jcursor dbpath(inodes().key(), fostlib::coerce<fostlib::string>(location));
    inodes().add(dbpath, path, path_hash,
        [
            self = this, inode_type, pred, builder, inoder, otherwise, dbpath,
            path = std::move(path), path_hash = std::move(path_hash)
        ](
            workers &w, fostlib::json &data, const fostlib::json &dbconf
        ) {
            if ( pred(data[dbpath]) ) {
                auto priority = tick::next();
                fostlib::json node;
                fostlib::insert(node, "filetype", inode_type);
                fostlib::insert(node, "name", path);
                fostlib::insert(node, "priority", priority);
                fostlib::insert(node, "hash", "name", path_hash);
                auto inode(inoder(priority, node));
                dbpath.replace(data, inode);
                rehash_inodes(w, dbconf);
                const auto sent = broadcast(
                    builder(self->tenant, priority, path, inode));
                fostlib::log::info(c_fost_rask)
                    ("", inode_type)
                    ("broadcast", "to", sent)
                    ("tenant", self->tenant.name())
                    ("node", inode);
            } else {
                const auto node = data[dbpath];
                auto inode(otherwise(node));
                if ( inode != node ) {
                    dbpath.replace(data, inode);
                    fostlib::log::info(c_fost_rask)
                        ("", inode_type)
                        ("tenant", self->tenant.name())
                        ("node", "old", node)
                        ("node", "new", inode);
                }
            }
        });
}
void rask::subscriber::local_change(
    const boost::filesystem::path &location,
    const fostlib::json &inode_type,
    condition_function pred, packet_builder builder, inode_function inoder
) {
    local_change(location, inode_type, pred, builder, inoder,
        [](const auto &i) { return i; });
}
void rask::subscriber::local_change(
    const boost::filesystem::path &location,
    const fostlib::json &inode_type,
    packet_builder builder, inode_function inoder
) {
    local_change(location, inode_type,
        [&inode_type](const fostlib::json &inode) {
            return inode["filetype"] != inode_type;
        }, builder, inoder);
}
void rask::subscriber::local_change(
    const boost::filesystem::path &location,
    const fostlib::json &inode_type,
    packet_builder builder
) {
    local_change(location, inode_type, builder,
        [](const rask::tick &priority, fostlib::json inode) {
            fostlib::digester hash(fostlib::sha256);
            hash << priority;
            fostlib::insert(inode, "hash", "inode",
                fostlib::coerce<fostlib::base64_string>(hash.digest()));
            return inode;
        });
}

// void rask::subscriber::remote_change(
//     const boost::filesystem::path &location,
//     const fostlib::json &inode_type,
//     const tick &priority, inode_function inoder
// ) {
//     auto path = relative_path(root, location);
//     auto path_hash = name_hash(path);
//     fostlib::jcursor dbpath(inodes().key(), fostlib::coerce<fostlib::string>(location));
//     inodes().add(dbpath, path, path_hash,
//         [
//             self = this, inode_type, priority, dbpath, inoder,
//             path = std::move(path), path_hash = std::move(path_hash)
//         ](
//             workers &w, fostlib::json &data, const fostlib::json &dbconf
//         ) {
//             if ( data[dbpath]["filetype"] != inode_type ||
//                     data[dbpath]["priority"].isnull() ||
//                     tick(data[dbpath]["priority"]) < priority ) {
//                 fostlib::json node;
//                 fostlib::insert(node, "filetype", inode_type);
//                 fostlib::insert(node, "name", path);
//                 fostlib::insert(node, "priority", priority);
//                 fostlib::insert(node, "hash", "name", path_hash);
//                 node = inoder(priority, node);
//                 dbpath.replace(data, node);
//                 rehash_inodes(w, dbconf);
//                 fostlib::log::info(c_fost_rask)
//                     ("", inode_type)
//                     ("tenant", self->tenant.name())
//                     ("path", "relative", path)
//                     ("node", node);
//             }
//         });
// }
// void rask::subscriber::remote_change(
//     const boost::filesystem::path &location,
//     const fostlib::json &inode_type,
//     const tick &priority
// ) {
//     remote_change(location, inode_type, priority,
//         [](const rask::tick &priority, fostlib::json inode) {
//             fostlib::digester hash(fostlib::sha256);
//             hash << priority;
//             fostlib::insert(inode, "hash", "inode",
//                 fostlib::coerce<fostlib::base64_string>(hash.digest()));
//             return inode;
//         });
// }


/*
    rask::subscriber::change
*/


struct rask::subscriber::change::impl {
    /// The subscription that this change deals with
    subscriber &sub;
    /// The that determines if the db entry is up to date or not
    std::function<bool(const fostlib::json &)> pred;
    /// Add in the new priority for the new inode data
    std::function<fostlib::json(fostlib::json, const fostlib::json&)> priority;
    /// Broadcast packet builder in case the database was updated
    std::function<void(rask::tenant &, const rask::tick &,
        const fostlib::string &, const fostlib::json &)> broadcast;
    /// Functions to be run after the transaction is committed
    std::vector<std::function<void(change &)>> post_commit;
    /// Functions to be run in the case where the database was updated
    /// and the transaction committed.
    std::vector<std::function<void(change &, fostlib::json)>> post_update;
    /// The tenant relative path
    fostlib::string relpath;
    /// The hash for the file name
    fostlib::string nhash;
    /// The file path on this server
    boost::filesystem::path location;
    /// The target inode type
    const fostlib::json &inode_target;
    /// The path into the database
    fostlib::jcursor dbpath;

    impl(
        subscriber &s, const fostlib::string &rp,
         const boost::filesystem::path &p, const fostlib::json &t
    ) :
        sub(s),
        pred([this](const fostlib::json &j) {
            return j["filetype"] != inode_target;
        }),
        priority([](fostlib::json n, const fostlib::json &) {
            fostlib::insert(n, "priority", tick::next());
            return n;
        }),
        broadcast([](auto &, const auto &, const auto &, const auto &) {}),
        relpath(rp),
        nhash(name_hash(relpath)),
        location(p),
        inode_target(t),
        dbpath(s.inodes().key(), fostlib::coerce<fostlib::string>(location))
    {
        if ( inode_target == tenant::file_inode ) {
            priority = [](fostlib::json n, const fostlib::json &o) {
                if ( o.has_key("priority") )
                    fostlib::insert(n, "priority", tick::next());
                return n;
            };
        }
    }
};


rask::subscriber::change::change(
    subscriber &s, const fostlib::string &rp, const boost::filesystem::path &p,
    const fostlib::json &t
) : pimpl(std::make_shared<impl>(s, rp, p, t)) {
}


rask::subscriber::change::~change() = default;


rask::subscriber &rask::subscriber::change::subscription() const {
    return pimpl->sub;
}


const boost::filesystem::path &rask::subscriber::change::location() const {
    return pimpl->location;
}


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
    pimpl->priority = [](fostlib::json n, const fostlib::json &) { return n; };
    return *this;
}
rask::subscriber::change &rask::subscriber::change::record_priority(
    const tick &t
) {
    pimpl->priority = [t](fostlib::json n, const fostlib::json &) {
        fostlib::insert(n, "priority", t);
        return n;
    };
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
            rask::broadcast(b(t, p, i, j));
        };
    return *this;
}


rask::subscriber::change &rask::subscriber::change::post_commit(
    std::function<void(change&)> f
) {
    pimpl->post_commit.push_back(f);
    return *this;
}


rask::subscriber::change &rask::subscriber::change::post_update(
    std::function<void(change &, fostlib::json)> f
) {
    pimpl->post_update.push_back(f);
    return *this;
}


void rask::subscriber::change::cancel() {
}
void rask::subscriber::change::execute() {
    pimpl->sub.inodes().add(pimpl->dbpath, pimpl->relpath, pimpl->nhash,
        [pimpl = this->pimpl](
            workers &w, fostlib::json &data, const fostlib::json &dbconf
        ) {
            auto logger(fostlib::log::debug(c_fost_rask));
            logger("", "rask::subscriber::change::execute()")
                ("dbpath", pimpl->dbpath);
            const bool entry = data.has_key(pimpl->dbpath);
            if ( entry ) {
                logger("node", "old", data[pimpl->dbpath]);
            }
            const fostlib::json old = entry ? data[pimpl->dbpath] : fostlib::json();
            if ( !entry || pimpl->pred(old) ) {
                logger("predicate", true);
                fostlib::json node;
                fostlib::insert(node, "filetype", pimpl->inode_target);
                fostlib::insert(node, "name", pimpl->relpath);
                fostlib::insert(node, "hash", "name", pimpl->nhash);
                node = pimpl->priority(node, old);
                logger("node", "new", node);
            } else {
                logger("predicate", false);
            }
        });
}

