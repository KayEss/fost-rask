/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/workers.hpp>

#include <fost/file>


namespace rask {


    /// Start a sweep for the tenant
    void start_sweep(workers &, std::shared_ptr<tenant>, const boost::filesystem::path &folder);


}

