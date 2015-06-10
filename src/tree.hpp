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
            /// A layer traversing the tree
            struct layer_type {
                beanbag::jsondb_ptr pdb;
                fostlib::jsondb::local meta;
                fostlib::json::const_iterator pos;
                fostlib::json::const_iterator end;

                layer_type(
                    beanbag::jsondb_ptr pdb, fostlib::jsondb::local meta,
                    const fostlib::json &p
                ) : pdb(pdb), meta(std::move(meta)), pos(p.begin()), end(p.end()) {
                }

                bool operator == (const layer_type &r) const {
                    return pdb == r.pdb && pos == r.pos && end == r.end;
                }
            };
            /// Hold the stack of layers
            std::vector<layer_type> layers;
            /// Construct an iterator
            const_iterator(const rask::tree &);
            /// Go to the beginning of the sequence
            void begin();
            /// Go to the end of the sequence
            void end();
            /// Check if we need to pop a layer
            void check_pop();
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

