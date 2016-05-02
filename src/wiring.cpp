/*
    Copyright 2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/wiring.hpp>
#include <f5/tamarind/boost-asio-reactor.hpp>
#include <f5/tamarind/frp.hpp>

#include <fost/log>


/*
    rask::wiring::local_server
*/


struct rask::wiring::local_server::impl {
    f5::input<tick> time;
    f5::input<server_hash> peer;
};


rask::wiring::local_server::local_server(f5::boost_asio::reactor_pool &rp)
: pimpl(new impl), block(rp),
    time(block, pimpl->time), peer(block, pimpl->peer)
{
}


rask::wiring::local_server::~local_server() = default;


