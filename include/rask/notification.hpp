/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <memory>
#include <boost/filesystem.hpp>


namespace rask {


    class tenant;
    struct workers;


    /// File system notifications
    class notification {
    public:
        /// Set up a notification handler
        notification(workers &);
        /// Destruct it
        ~notification();

        /// Start processing the notifications
        void operator() ();

        /// Add a watch for a directory
        bool watch(std::shared_ptr<tenant>, const boost::filesystem::path &);

    private:
        struct impl;
        std::unique_ptr<impl> pimpl;
    };


}

