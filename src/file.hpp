/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/datetime>

#include <boost/filesystem/path.hpp>


namespace rask {


    /// Ensures that the filename requested is large enough to store the
    /// requested number of bytes. The file is enlarged (padded with zero)
    /// or shrunk as needed.
    void allocate_file(const boost::filesystem::path &, std::size_t);

    /// File stat structure giving relevant types
    struct stat {
        stat(const stat &) = default;
        stat(int64_t, fostlib::timestamp);

        int64_t size;
        fostlib::timestamp modified;
    };

    /// Return the stat information about a file. This allows size and
    /// modified time to be returned from one syscall.
    rask::stat file_stat(const boost::filesystem::path &);


}

