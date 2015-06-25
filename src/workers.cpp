/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/configuration.hpp>
#include <rask/workers.hpp>

#include <fost/log>


namespace {
    auto eh() {
        return []() {
            fostlib::log::critical(rask::c_fost_rask, "Rask pool thread caught an exception");
            fostlib::absorb_exception();
            return true;
        };
    }
}


rask::workers::workers()
: low_latency(eh(), 4), high_latency(eh(), 4), notify(*this) {
}

