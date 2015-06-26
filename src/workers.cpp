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
            auto eptr = std::current_exception();
            if ( eptr ) {
                try {
                    std::rethrow_exception(eptr);
                } catch ( fostlib::exceptions::exception &e ) {
                    fostlib::log::critical(rask::c_fost_rask)
                        ("", "Rask thread pool caught an exception")
                        ("message", e.message())
                        ("data", e.data());
                } catch ( std::exception &e ) {
                    fostlib::log::critical(rask::c_fost_rask)
                        ("", "Rask thread pool caught an exception")
                        ("what", e.what());
                } catch ( ... ) {
                    fostlib::log::critical(rask::c_fost_rask,
                        "Rask thread pool caught an unknown exception");
                }
                fostlib::absorb_exception();
                return true;
            } else
                return false;
        };
    }
}


rask::workers::workers()
: low_latency(eh(), 4), high_latency(eh(), 4), notify(*this) {
}

