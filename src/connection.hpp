/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/internet>


namespace rask {


    struct workers;


    /// A connection between two Rask servers
    class connection {
    public:
        /// Construct a connection
        connection(workers &);

        /// The socket used for this connection
        boost::asio::ip::tcp::socket cnx;
        /// Strand used for sending
        boost::asio::io_service::strand sender;
        /// An input buffer

        /// Send a version block
        void version();
    };


    /// Read and process a packet
    void read_and_process(std::shared_ptr<connection>);


}

