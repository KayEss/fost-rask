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
            friend class subscriber;
            /// The subscription that this change deals with
            subscriber &m_sub;
            /// The that determines if the db entry is up to date or not
            std::function<bool(const fostlib::json &)> m_pred;
            /// Add in the new priority for the new inode data
            std::function<fostlib::json(fostlib::json, const fostlib::json&)> m_priority;
            /// Broadcast packet builder in case the database was updated
            std::function<void(rask::tenant &, const rask::tick &,
                const fostlib::string &, const fostlib::json &)> m_broadcast;
            /// Functions to be run after the transaction is committed
            std::vector<std::function<void(change &)>> m_post_commit;
            /// Functions to be run in the case where the database was updated
            /// and the transaction committed.
            std::vector<std::function<void(change &, fostlib::json)>> m_post_update;

            /// Construct the change
            change(
                subscriber &,
                const fostlib::string &,
                const boost::filesystem::path &,
                const fostlib::json &);
        public:
            /// Show an error in the log stream if the change was neither
            /// cancelled or executed
            ~change();

            /// Allow access to the subscriber
            subscriber &subscription() {
                return m_sub;
            }

            /// The tenant relative path
            fostlib::accessors<const fostlib::string> relpath;
            /// The hash for the file name
            fostlib::accessors<const fostlib::string> nhash;
            /// The file path on this server
            fostlib::accessors<const boost::filesystem::path> location;
            /// The target inode type
            fostlib::accessors<const fostlib::json &> inode_target;
            /// The path into the database that this inode data will live
            /// at.
            fostlib::accessors<const fostlib::jcursor> dbpath;

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

            /// A post commit hook is run after the database transaction
            /// has completed whether or not there was an update
            change &post_commit(std::function<void(change&)>);
            /// This hook is executed only when the database was updated
            change &post_update(std::function<void(change &)> f) {
                return post_update(
                    [f](auto &c, auto) {
                        f(c);
                    });
            }
            /// This hook is also provided with the new JSON for the inode
            change &post_update(std::function<void(change &, fostlib::json)>);
            /// Broadcast a packet when the change has been recorded
            change &broadcast(std::function<
                connection::out(rask::tenant &, const rask::tick &,
                        const fostlib::string &, const fostlib::json &)>);

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

        /// Write details about something observed on this file system. After
        /// recording in the database it will build a packet and broadcast it to
        /// the other connected nodes
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            packet_builder);
        /// Write details about something observed on this file system. This version
        /// allows for custom hashing of the item if it's added to the database
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            packet_builder, inode_function);
        /// Write details about something observed on this file system. This version
        /// allows for custom hashing and also a custom predicate to determine if
        /// the inode data entry needs to be updated
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            condition_function, packet_builder, inode_function);
        /// Write details about something observed on this file system. This
        /// version allows for custom hashing, a custom predicate to deterime if
        /// the inode data entry needs to be updated plus a secondary updater
        /// that allows other information in the inode to be updated if required.
        void local_change(
            const boost::filesystem::path &location,
            const fostlib::json &inode_type,
            condition_function, packet_builder, inode_function, otherwise_function);

//         /// Record a change that has come from another server
//         void remote_change(
//             const boost::filesystem::path &location,
//             const fostlib::json &inode_type, const tick &);
//         void remote_change(
//             const boost::filesystem::path &location,
//             const fostlib::json &inode_type, const tick &,
//             inode_function);

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

