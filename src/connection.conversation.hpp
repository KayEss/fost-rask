/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include "connection.hpp"


namespace rask {


    /// Class for storing conversation state
    class connection::conversation {
        /// Store the socket so we can get at it
        std::shared_ptr<connection> socket;
    public:
        /// Construct from a connection
        conversation(std::shared_ptr<connection>);
        /// Destructor so we can reset the conversing flag
        ~conversation();

        /// Send information about tenants
        void tenants(std::shared_ptr<conversation>);
    };


}

