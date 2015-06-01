/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/internet>
#include <fost/jsondb>
#include <fost/log>

#include <boost/asio/streambuf.hpp>
#include <boost/endian/conversion.hpp>

#include <atomic>
#include <iostream>


namespace rask {


    class tenant;
    class tick;
    struct workers;


    /// The highest version of the wire protocol this code knows
    const char known_version = 0x01;


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

        class out {
            /// Output buffers
            std::unique_ptr<boost::asio::streambuf> buffer;
            /// The control block value
            unsigned char control;
        public:
            /// Construct an outbound packet
            out(unsigned char control)
            : buffer(new boost::asio::streambuf), control(control) {
            }
            /// Move constructor
            out(out&& o)
            : buffer(std::move(o.buffer)), control(o.control) {
            }

            /// Insert an integer in network byte order
            template<typename I,
                typename = std::enable_if_t<std::is_integral<I>::value>>
            out &operator << (I i) {
                if ( sizeof(i) > 1 ) {
                    auto v = boost::endian::native_to_big(i);
                    buffer->sputn(reinterpret_cast<char *>(&v), sizeof(v));
                } else {
                    buffer->sputc(i);
                }
                return *this;
            }
            /// Insert a clock tick on the buffer
            out &operator << (const tick &);
            /// Insert a string on the buffer
            out &operator << (const fostlib::string &);
            /// Insert a fixed size memory block. If the size is not fixed then it
            /// needs to be prefixed with a size_sequence so the remote end
            /// knows how much data has been sent
            out &operator << (const fostlib::const_memory_block);

            /// Return the current size of the packet
            std::size_t size() const {
                return buffer->size();
            }

            /// Add a size control sequence
            out &size_sequence(std::size_t s) {
                size_sequence(s, *buffer);
                return *this;
            }

            /// Put the data on the wire
            void operator () (std::shared_ptr<connection> socket) const {
                (*this)(socket, [](){});
            }
            /// Put the data on the wire, then call the requested callback
            template<typename CB>
            void operator () (std::shared_ptr<connection> socket, CB cb) const {
                auto sender = socket->sender.wrap(
                    [socket, cb](const boost::system::error_code &error, std::size_t bytes) {
                        if ( error ) {
                            fostlib::log::error()
                                ("", "Error sending data packet")
                                ("connection", socket->id)
                                ("error", error.message().c_str());
                        } else {
                            cb();
                        }
                    });
                boost::asio::streambuf header;
                size_sequence(size(), header);
                header.sputc(control);
                std::array<boost::asio::streambuf::const_buffers_type, 2> data{
                    header.data(), buffer->data()};
                async_write(socket->cnx, data, sender);
            }

        private:
            /// Write a size control sequence to the specified buffer
            static void size_sequence(std::size_t, boost::asio::streambuf &);
        };
    };


    /// Broadcast a packet to all connections -- return how many were sent
    std::size_t broadcast(const connection::out &packet);

    /// Monitor the connection
    void monitor_connection(std::shared_ptr<connection>);
    /// Read and process a packet
    void read_and_process(std::shared_ptr<connection>);

    /// Send a version packet with heartbeat
    void send_version(std::shared_ptr<connection>);
    /// Process a version packet body
    void receive_version(std::shared_ptr<connection>, std::size_t);

    /// Send a create directory instruction
    connection::out create_directory(
        tenant &, const rask::tick &, fostlib::jsondb::local &, const fostlib::jcursor &,
        const fostlib::string &name);


}

