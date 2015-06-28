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


    class tree;


    /// Parent class for all blocks
    class block {
    protected:
        /// Allow the sub-class to construct the block
        block(const tree &, block *, std::size_t, name_hash_type);

    public:
        /// Allow safe sub-classing
        virtual ~block() = default;

        /// Set this true when the block needs to be re-hashed
        std::atomic<bool> dirty;
        /// The depth of the block in the tree
        const std::size_t depth;
        /// The hash prefix for this part of the tree
        const name_hash_type prefix;

    private:
        /// The parent block to this one -- is nullptr for the root block
        const block *parent;
        /// The tree that this block is part of
        const tree &part_of;
    };


    /// A block of leaves
    template<typename L>
    class leaf_block : public block {
        f5::tsmap<fostlib::string, L> leaves;
    public:
        /// The type of leaf
        using leaf_type = L;
        using leaf_weak_ptr = std::weak_ptr<leaf_type>;
        /// A block of leaves
        using leaves_type = f5::tsmap<fostlib::string, leaf_weak_ptr>;
    };


    /// A mid-tree block
    template<typename L>
    class mid_block : public block {
    protected:
        mid_block(const tree &t, block *b, std::size_t d, name_hash_type px)
        : block(t, b, d, std::move(px)) {
        }
    public:
    };


    /// A root block
    template<typename L>
    class root_block : public mid_block<L> {
        using mid_blocks = f5::tsmap<
            name_hash_type, std::shared_ptr<mid_block<L>>>;
        f5::tsmap<std::size_t, mid_blocks> mids;
    public:
        /// Construct a root block
        root_block(const tree &t)
        : mid_block<L>(t, nullptr, 0u, name_hash_type()) {
        }
    };


}

