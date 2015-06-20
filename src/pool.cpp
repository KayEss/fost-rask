/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/configuration.hpp>
#include <rask/pool.hpp>

#include <fost/log>

#include <thread>
#include <vector>


using namespace fostlib;
namespace asio = boost::asio;


struct rask::pool::impl {
    impl(rask::pool &p)
    : work(new asio::io_service::work(p.io_service)) {
    }

    std::vector<std::thread> threads;
    std::unique_ptr<asio::io_service::work> work;
};


rask::pool::pool(std::size_t threads)
: pimpl(new impl(*this)) {
    for ( auto t = 0u; t != threads; ++t ) {
        pimpl->threads.emplace_back([this]() {
            bool again = false;
            do {
                try {
                    again = false;
                    io_service.run();
                } catch ( fostlib::exceptions::exception &e ) {
                    again = true;
                    log::critical(c_fost_rask)
                        ("", "Rask pool thread caught an exception")
                        ("what", e.what())
                        ("data", e.data());
                    fostlib::absorb_exception();
                } catch ( std::exception &e ) {
                    again = true;
                    log::critical(c_fost_rask, "Rask pool thread caught an exception", e.what());
                    fostlib::absorb_exception();
                } catch ( ... ) {
                    again = true;
                    log::critical(c_fost_rask, "Rask pool thread caught an exception");
                    fostlib::absorb_exception();
                }
            } while (again);
        });
    }
}


rask::pool::~pool() {
    log::debug(c_fost_rask,  "Terminating thread pool");
    pimpl->work.reset();
    io_service.stop();
    std::for_each(pimpl->threads.begin(), pimpl->threads.end(), [](auto &t){ t.join(); });
}
