/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "hash-block.hpp"
#include "tree.hpp"
#include <rask/configuration.hpp>

#include <fost/counter>


rask::block::block(const tree &t, block *p, std::size_t d, name_hash_type px)
: part_of(t), parent(p), depth(d), prefix(std::move(px)) {
}

