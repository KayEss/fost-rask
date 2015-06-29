/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include "hash.hpp"

#include <f5/threading/map.hpp>

#include <fost/core>

#include <mutex>
#include <vector>


namespace rask {


    class root_block;
    class tree;


    /// Parent class for all blocks
    class block {
    protected:
        /// Allow the sub-class to construct the block
        block(const tree &, block *, std::size_t, name_hash_type);

        /// A mutex that can be used to control access. Some changes we wish
        /// to make require us to grab a mutex on the block that is being
        /// worked on. Other times we need to grab a lock on the mutex for
        /// the parent block.
        std::mutex mutex;
    public:
        /// Allow safe sub-classing
        virtual ~block() = default;

        /// Enumeration for the state of the block
        enum class state { loaded, dirty, hashing, save };
        /// Set this true when the block needs to be re-hashed
        std::atomic<state> current;
        /// The depth of the block in the tree
        const std::size_t depth;
        /// The hash prefix for this part of the tree
        const name_hash_type prefix;

    protected:
        /// The parent block to this one -- is nullptr for the root block
        const block *parent;
        /// The tree that this block is part of
        const tree &partof;
    };


    /// A block of leaves
    class leaf_block : public block {
        friend root_block;
        leaf_block(const tree &t, block *b, std::size_t d, name_hash_type px);
    public:
    };


    /// A mid-tree block
    class mid_block : public block {
        friend root_block;
        mid_block(const tree &t, block *b, std::size_t d, name_hash_type px);
    public:
    };


    /// A root block
    class root_block : public block {
        /// Stores the actual block itself because we don't statically know
        /// if the root has been partitioned yet
        std::unique_ptr<block> actual;
    public:
        /// Construct a root block
        root_block(const tree &);
    };


}

