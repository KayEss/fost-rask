/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include "hash-block.hpp"
#include <beanbag/beanbag>


namespace rask {


    struct workers;


    /// Value for the @context member of the JSON when it represents
    /// a partitioned layer in the tree.
    extern const fostlib::json c_db_cluster;


    /// Given either a local transaction or JSON returns true if it represents
    /// a partitioned data layer.
    template<typename D>
    inline bool partitioned(const D &d) {
        return d["@context"] == c_db_cluster;
    }


    /// Class that can be used to provide an interface onto the beanbags
    /// needed to implement the tree for large collections of names -- tenants
    /// and inodes.
    class tree {
        /// The workers that can be used for async jobs
        rask::workers &workers;
        /// The beanbag configuration for the root database
        const fostlib::json root_db_config;
        /// The root jcursor that we are going to iterate on
        const fostlib::jcursor root;

    public:
        /// Construct the tree
        tree(rask::workers &, fostlib::json config, fostlib::jcursor root,
            fostlib::jcursor name_hash, fostlib::jcursor item_hash);

        /// The path within an entry where we can find the hash
        const fostlib::accessors<fostlib::jcursor> name_hash_path;
        /// The path within an entry where we can find the item hash
        const fostlib::accessors<fostlib::jcursor> item_hash_path;

        /// Return the root database
        beanbag::jsondb_ptr root_dbp() const;
        /// Return the key that is being managed
        const fostlib::jcursor &key() const {
            return root;
        }
        /// Return the database config for a particular layer
        fostlib::json layer_db_config(std::size_t layer, const name_hash_type &) const;
        /// Return the database for a particualr layer
        beanbag::jsondb_ptr layer_dbp(
            std::size_t layer, const name_hash_type &hash
        ) const {
            return beanbag::database(layer_db_config(layer, hash));
        }

        /// Stores the blocks and the hashes. For now store the JSON for the
        /// inode at the leaf positions.
        root_block hash;

        /// The type of the manipulator that runs inside the node database
        using manipulator_fn =
            std::function<void(rask::workers &, fostlib::json &, const fostlib::json &)>;
        /// Return a local transaction that covers the database where we
        /// want to add a node
        void add(const fostlib::jcursor &dbpath,
            const fostlib::string &path, const name_hash_type &hash,
            manipulator_fn);

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
            void begin(beanbag::jsondb_ptr dbp);
            /// Go to the end of the sequence
            void end();
            /// Check if we need to pop a layer. Returns true if a layer was popped.
            bool check_pop();
            // Recurse down one layer
            void down();
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

