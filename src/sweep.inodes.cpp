/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "sweep.inodes.hpp"
#include "tree.hpp"
#include <rask/tenant.hpp>


namespace {
    struct closure {
        std::shared_ptr<rask::tenant> tenant;
        boost::filesystem::path folder;
        rask::tree::const_iterator position;
        rask::tree::const_iterator end;

        closure(std::shared_ptr<rask::tenant> t, boost::filesystem::path f)
        : tenant(t), folder(std::move(f)),
                position(t->inodes().begin()), end(t->inodes().end()) {
        }
    };
    void block(boost::asio::io_service &s, std::shared_ptr<closure> c) {
        while ( c->position != c->end ) {
            auto inode = *c->position;
            ++c->position;
        }
    }
}


void rask::sweep_inodes(
    workers &w, std::shared_ptr<tenant> t, boost::filesystem::path f
) {
    auto c = std::make_shared<closure>(t, std::move(f));
    w.high_latency.io_service.post(
        [&io_service = w.high_latency.io_service, c]() {
            block(io_service, c);
        });
}

