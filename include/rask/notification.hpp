/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <memory>


namespace rask {


    struct workers;


    /// File system notifications
    class notification {
    public:
        /// Set up a notification handler
        notification();
        /// Destruct it
        ~notification();

        /// Start processing the notifications
        void operator() (workers &);

    private:
        struct impl;
        std::unique_ptr<impl> pimpl;
    };


}

