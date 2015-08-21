/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <boost/filesystem/path.hpp>


namespace rask {


    /// Ensures that the filename requested is large enough to store the
    /// requested number of bytes. The file is enlarged (padded with zero)
    /// or shrunk as needed.
    void allocate_file(const boost::filesystem::path &, std::size_t);


}

