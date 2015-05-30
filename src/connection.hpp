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
        static std::atomic<int64_t> g_id;
    public:
        /// Construct a connection
        connection(boost::asio::io_service&);
        /// Destructor so we can log failed connections
        ~connection();

        /// The connection ID used in log messages
        const int64_t id;

        /// The socket used for this connection
        boost::asio::ip::tcp::socket cnx;
        /// Strand used for sending
        boost::asio::io_service::strand sender;
        /// An input buffer
        boost::asio::streambuf input_buffer;
        /// Heartbeat timer
        boost::asio::deadline_timer heartbeat;

        /// Structure used to manage
        class reconnect {
        public:
            reconnect(workers &, const fostlib::json &);
            /// The network configuration to be used to connect
            fostlib::json configuration;
            /// The watchdog timer that will be responsible for reconnecting
            boost::asio::deadline_timer watchdog;
            /// Allow the watchdog to cancel the current connection if it can
            std::weak_ptr<connection> socket;
        };

        /// Store the reconnect so the watchdog can be reset
        std::shared_ptr<reconnect> restart;
    };


    /// Monitor the connection
    void monitor_connection(std::shared_ptr<connection>);
    /// Read and process a packet
    void read_and_process(std::shared_ptr<connection>);


}

