/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <beanbag/beanbag>


namespace rask {


    /// Class that can be used to provide an interface onto the beanbags
    /// needed to implement the tree for large collections of names -- tenants
    /// and inodes.
    class tree {
        /// The beanbag configuration for the root database
        const fostlib::json root_db_config;
        /// The root jcursor that we are going to iterate on
        const fostlib::jcursor root;
    public:
        /// Construct the tree
        tree(fostlib::json config, fostlib::jcursor root, fostlib::jcursor hash);

        /// The path within an entry where we can find the hash
        const fostlib::accessors<fostlib::jcursor> name_hash_path;

        /// Return the root database
        beanbag::jsondb_ptr root_dbp() const;
        /// Return the key that is being managed
        const fostlib::jcursor &key() const {
            return root;
        }

        /// Return a local transaction that covers the database where we
        /// want to add a node
        fostlib::jsondb::local add(const fostlib::jcursor &dbpath,
            const fostlib::string &path, const std::vector<unsigned char> &hash);

        class const_iterator {
            friend class rask::tree;
            /// The owning tree
            const rask::tree &tree;
            /// For now just hold the root
            beanbag::jsondb_ptr root_dbp;
            /// A local transaction for getting into the data
            fostlib::jsondb::local root_data;
            /// For now just hold the undelying iterator
            fostlib::json::const_iterator underlying;
            /// Construct an iterator
            const_iterator(const rask::tree &, beanbag::jsondb_ptr dbp);
            /// Go to the beginning of the sequence
            void begin();
            /// Go to the end of the sequence
            void end();
        public:
            /// Move constructor
            const_iterator(const_iterator &&);

            /// Return the current JSON
            fostlib::json operator * () const;

            /// Move to the next item
            const_iterator &operator ++ ();

            /// Check two iterators for equality
            bool operator == (const const_iterator &) const;
        };

        /// Return an iterator to the first of the underlying items
        const_iterator begin() const;
        /// Return an iterator to the end of the underlying items
        const_iterator end() const;
    };


}

