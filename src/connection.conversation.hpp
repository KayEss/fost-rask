/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include "connection.hpp"

#include <beanbag/beanbag>


namespace rask {


    class peer;


    /// Class for storing conversation state
    class connection::conversation {
        /// The tenants database
        beanbag::jsondb_ptr tenants_dbp;
        /// Store the socket so we can get at it
        std::shared_ptr<connection> socket;
        /// The remote peer we're talking to
        peer &partner;
    public:
        /// Construct from a connection
        conversation(std::shared_ptr<connection>);
        /// Destructor so we can reset the conversing flag
        ~conversation();

        /// Send information about tenants
        static void tenants(std::shared_ptr<conversation>);
    };


}

