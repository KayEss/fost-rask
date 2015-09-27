/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <rask/configuration.hpp>
#include <rask/clock.hpp>

#include <fost/internet>
#include <fost/jsondb>
#include <fost/log>

#include <f5/threading/ring.hpp>

#include <boost/asio/streambuf.hpp>
#include <boost/endian/conversion.hpp>

#include <atomic>


namespace rask {


    class const_file_block_hash_iterator;
    class tenant;
    class tick;
    struct workers;


    /// The name hash
    using name_hash_type = fostlib::string;


    /// The highest version of the wire protocol this code knows
    const char known_version = 0x01;


    /// A connection between two Rask servers
    class connection : public std::enable_shared_from_this<connection> {
        static std::atomic<int64_t> g_id;
    public:
        /// Construct a connection
        connection(rask::workers&);
        /// Destructor so we can log failed connections
        ~connection();

        /// Worker pool used for this connection
        rask::workers &workers;
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
            reconnect(rask::workers &, const fostlib::json &);
            /// The network configuration to be used to connect
            fostlib::json configuration;
            /// The watchdog timer that will be responsible for reconnecting
            boost::asio::deadline_timer watchdog;
            /// Allow the watchdog to cancel the current connection if it can
            std::weak_ptr<connection> socket;
        };

        /// Store the reconnect so the watchdog can be reset
        std::shared_ptr<reconnect> restart;


        /// The identity of the server we're connected with
        std::atomic<uint32_t> identity;


        /// Build an outbound packet
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
            /// Insert a string on the buffer, with its size header
            out &operator << (const fostlib::string &);
            /// Insert a fixed size memory block. If the size is not fixed then it
            /// needs to be prefixed with a size_sequence so the remote end
            /// knows how much data has been sent
            out &operator << (const fostlib::const_memory_block);
            /// Insert a vector of bytes onto the stream. No size prefix is written.
            out &operator << (const std::vector<unsigned char> &);

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
                    [socket, cb](
                        const boost::system::error_code &error, std::size_t bytes
                    ) {
                        if ( error ) {
                            fostlib::log::error(c_fost_rask)
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
                std::array<boost::asio::streambuf::const_buffers_type, 2>
                    data{{header.data(), buffer->data()}};
                async_write(socket->cnx, data, sender);
            }

        private:
            /// Write a size control sequence to the specified buffer
            static void size_sequence(std::size_t, boost::asio::streambuf &);
        };

        /// Queue a packet for outbound sending. Can be called from multiple
        /// threads.
        void queue(std::function<out(void)>);

        /// Allows a network connection to be read from
        class in {
            friend void read_and_process(std::shared_ptr<connection>);
            /// Construct
            in(std::shared_ptr<connection> socket, std::size_t s);
            /// Throw an EOF exception if there isn't enough data
            void check(std::size_t) const;
        public:
            /// The connection we're reading from
            std::shared_ptr<connection> socket;

            /// Destructor clears unprocessed input
            ~in();

            /// Return true if the packet is empty
            bool empty() const {
                return remaining == 0u;
            }
            /// Return the connection ID
            int64_t socket_id() const {
                return socket->id;
            }

            /// Read a size  control sequence
            std::size_t size_control();

            /// Read an integer
            template<typename I,
                typename std::enable_if<std::is_integral<I>::value>::type* = nullptr>
            I read() {
                I i;
                check(sizeof(i));
                if ( sizeof(i) > 1 ) {
                    socket->input_buffer.sgetn(reinterpret_cast<char *>(&i), sizeof(i));
                    boost::endian::big_to_native_inplace(i);
                } else {
                    i = socket->input_buffer.sbumpc();
                }
                remaining -= sizeof(i);
                return i;
            }
            /// Read a clock tick
            template<typename T,
                typename std::enable_if<std::is_same<tick, T>::value>::type* = nullptr>
            T read() {
                auto time(read<int64_t>());
                auto server(read<uint32_t>());
                return tick::overheard(time,server);
            }
            /// Read a string
            template<typename T,
                typename std::enable_if<std::is_same<fostlib::string, T>::value>::type* = nullptr>
            T read() {
                auto data = read(size_control());
                return fostlib::coerce<fostlib::string>(
                    fostlib::coerce<fostlib::utf8_string>(data));
            }
            /// Read a number of bytes
            std::vector<unsigned char> read(std::size_t b);

        private:
            /// The number of bytes remaining to be read
            std::size_t remaining;
        };

    private:
        /// Buffer of outbound packets
        f5::tsring<std::function<rask::connection::out(void)>> packets;
        /// Send the head of the queue
        void send_head();
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
    void receive_version(connection::in &);

    /// Build a tenant instruction
    connection::out tenant_packet(const fostlib::string &name,
            const fostlib::json &meta);
    /// Build a packet of a set of hashes in the tenant hash tree
    connection::out tenant_packet(tenant &,
            std::size_t layer, const name_hash_type &, const fostlib::json &);
    /// React to a tenant that has come in
    void tenant_packet(connection::in &packet);
    /// React to a tenant hash that has come in
    void tenant_hash_packet(connection::in &packet);

    /// Send a create directory instruction
    connection::out create_directory_out(
        tenant &, const rask::tick &, const fostlib::string &, const fostlib::json &);
    /// React to a directory create request
    void create_directory(connection::in &);

    /// Send a file exists instruction
    connection::out file_exists_out(
        tenant &, const rask::tick &, const fostlib::string &, const fostlib::json &);
    /// React to a file exists instruction
    void file_exists(connection::in &);

    /// An empty file hash packet without priority
    connection::out send_empty_file_hash(tenant &, const fostlib::json &);
    /// Recieve a file hash packet
    void file_hash_without_priority(connection::in &);
    /// Send a file data block
    connection::out send_file_block(tenant &, const tick &,
        const fostlib::string &, const boost::filesystem::path &,
        const const_file_block_hash_iterator &);
    /// Process a data block
    void file_data_block(connection::in &);

    /// Create a move an inode out packet
    connection::out move_out_packet(
        tenant &, const rask::tick &, const fostlib::string &, const fostlib::json &);
    /// React to a move inode out packet
    void move_out(connection::in &);


}

