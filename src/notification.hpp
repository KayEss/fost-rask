/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include<boost/filesystem.hpp>


namespace rask {


    class tenant;
    struct workers;


    void rm_directory(workers &, std::shared_ptr<tenant>,
        const boost::filesystem::path &);


}
