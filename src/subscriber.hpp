/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/file>

#include <rask/connection.hpp>

#include <beanbag/beanbag>


namespace rask {


    class tenant;
    class tick;
    class tree;


    /// The type of a function used to build inode packets
    using packet_builder = std::function<
        connection::out(tenant &, const rask::tick &, const fostlib::string &,
            const fostlib::json &)>;
    /// The type of the function that tells the subscription whether it needs to
    /// update or not
    using condition_function = std::function<bool(const fostlib::json &inode)>;
    /// The type of the function used to re-write the inode before it is saved
    using inode_function = std::function<
        fostlib::json(const rask::tick &, fostlib::json)>;
    /// The type of the function used to manipulate the database entry if
    /// the entry is not being replaced
    using otherwise_function = std::function<fostlib::json(fostlib::json)>;


    /// The part of the tenant that is a subscriber
    class subscriber {
    public:
        /// Used for internal caclulations
        const fostlib::string root;
        /// The tenant that this subscription is part of
        rask::tenant &tenant;

        /// Constructs a subscriber by providing a local path and the tenant
        subscriber(workers &, rask::tenant &, fostlib::string local);

        /// The local filesystem path
        const fostlib::accessors<boost::filesystem::path> local_path;

        /// The tenant beanbag with file system information
        beanbag::jsondb_ptr beanbag() const;

        /// Used to configure the behaviour of a change. Different sorts
        /// of changes coming from different places can build an instance
        /// of this that handles the differences from the base behaviour
        class change {
            struct impl;
            std::shared_ptr<impl> pimpl;
            friend class subscriber;
            /// Construct the change
            change(
                subscriber &,
                const fostlib::string &,
                const boost::filesystem::path &,
                const fostlib::json &);
        public:
            /// Change status is used to report on what happened to the
            /// post change callbacks
            struct status {
                /// The subscription that this change deals with
                subscriber &subscription;
                /// The file path on this server
                boost::filesystem::path location;
                /// Assigned true/false depending on whether the predicatae has told
                /// us to do the database update or not
                bool updated;
                /// Copy of the old node data (if any)
                fostlib::json old;
                /// Copy of the new inode data (if set)
                fostlib::json inode;

            private:
                friend struct change::impl;
                status(subscriber &, boost::filesystem::path);
            };

            /// Show an error in the log stream if the change was neither
            /// cancelled or executed
            ~change();

            /// Set extra predicate conditions that must also be true
            /// in order that we record a new node in the database.
            /// Predicates are chained together through a logical `or` so
            /// that the first one that returns `true` will cause the
            /// database to be updated. The `inode_target` is always
            /// checked and a mismatch there will always trigger a
            /// database update no matter what other predicates are
            /// also given.
            change &predicate(std::function<bool(const fostlib::json &)>);

            /// Record the priority that we want to check the change
            /// against. This becomes the default update priority and is
            /// registered as a predicate as above.
            change &compare_priority(const tick &);
            /// Record no priority for the change. If we wish to not
            /// update the priroty value this must be set after the
            /// `compare_prority`.
            change &record_priority(fostlib::t_null);
            /// Record the specified clock tick as the priority
            change &record_priority(const tick &);
            /// Return the priority to update given the old inode
            change &record_priority(std::function<tick(const fostlib::json &)>);

            /// Provide a function that generates the hash for the node. The
            /// default function will generate a hash based on the tick.
            change &hash(std::function<
                fostlib::json(const tick &, const fostlib::json &inode)>);

            /// Allow us to store extra information in the JSON when we
            /// update the node database.
            change &enrich_update(std::function<fostlib::json(fostlib::json)>);
            /// Allow us to store extra information in the JSON when we
            /// do not update the database
            change &enrich_otherwise(std::function<fostlib::json(fostlib::json)>);

            /// Broadcast a packet when the change has been recorded. This
            /// function will only be called if a clock tick is recorded in an
            /// update
            change &broadcast(std::function<
                connection::out(rask::tenant &, const rask::tick &,
                        const fostlib::string &, const fostlib::json &)>);

            /// These hook is executed only when the database was updated
            change &post_update(std::function<void(const status &)>);
            /// These hook is executed only when the database is not updated
            change &post_otherwise(std::function<void(const status &)>);
            /// A post commit hook is run after the database transaction
            /// has completed whether or not there was an update. These are
            /// all called after the post_update functions.
            change &post_commit(std::function<void(const status &)>);

            /// Mark the job as cancelled. This stops logging of an error
            /// and also causes any future call to `execute` to error
            /// as well
            void cancel();
            /// The final step is to execute the change once we have
            /// fully configured its behaviour.
            void execute();
        };
        /// Construct an initial change which attempts to make sure that
        /// the target inode type is met. The file location is relative to the
        /// current path (i.e. it can be used at any time to as a path to
        /// examine the actual disk content)
        change file(const boost::filesystem::path &location);
        change directory(const boost::filesystem::path &location);
        change move_out(const boost::filesystem::path &location);
        /// Construct a change which takes a fostlib::string to the
        /// tenant relative path. This is what we get when we pull a
        /// filename off a network connection
        change file(const fostlib::string &relpath);
        change directory(const fostlib::string &relpath);
        change move_out(const fostlib::string &relpath);

        /// The inodes
        const tree &inodes() const {
            return *inodes_p;
        }
        tree &inodes() {
            return *inodes_p;
        }

    private:
        std::unique_ptr<tree> inodes_p;
    };


}

