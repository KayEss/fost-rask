/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "file.hpp"
#include <rask/configuration.hpp>

#include <fost/log>
#include <fost/exception/unexpected_eof.hpp>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include <system_error>

#include <fcntl.h>
#include <sys/stat.h>


namespace {


    template<typename F> inline
    int syscall(F f) {
        int result{};
        do {
            result = f();
        } while ( result == -1 && errno == EINTR );
        return result;
    }


}


fostlib::string rask::relative_path(
    const fostlib::string &root, const boost::filesystem::path &location
) {
    auto path = fostlib::coerce<fostlib::string>(location);
    if ( path.startswith(root) ) {
        path = path.substr(root.length());
    } else {
        fostlib::exceptions::not_implemented error(
            "Directory is not in tenant root");
        fostlib::insert(error.data(), "root", root);
        fostlib::insert(error.data(), "location", location);
        throw error;
    }
    return path;
}


void rask::allocate_file(const boost::filesystem::path &fn, std::size_t size) {
    if ( boost::filesystem::exists(fn) ) {
        boost::filesystem::resize_file(fn, size);
    } else {
        boost::filesystem::create_directories(fn.parent_path());
        if ( size == 0 ) {
            /// We can't use the fallocate system call for a zero
            /// byte file.
            fostlib::utf::save_file(fn, fostlib::string());
            return;
        }
        // Resize the file, probably overkill on the complexity front
        int opened = syscall([&]() {
                const int flags = O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW;
                // user read/write, group read/write, world read
                const int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
                return open(fn.c_str(), flags, mode);
            });
        if ( opened >= 0 ) {
            const int alloc = syscall([&]() {
                    const int mode = 0u;
                    const off_t offset = 0u;
                    return fallocate(opened, mode, offset, size);
                });
            const auto alloc_err = errno;
            syscall([&]() { return close(opened); });
            if ( alloc == -1 ) {
                std::error_code error(alloc_err, std::system_category());
                fostlib::log::error(c_fost_rask)
                    ("", "rask::allocate_file - fallocate")
                    ("filename", fn)
                    ("size", size)
                    ("error",  error.message().c_str());
                throw fostlib::exceptions::unexpected_eof(
                    "Could not change allocate size of the hash database file",
                    error.message().c_str());
            }
        } else {
            std::error_code error(errno, std::system_category());
            fostlib::log::error(c_fost_rask, "open", error.message().c_str());
            throw fostlib::exceptions::not_implemented(
                "Bad file descriptor for hash database file", error.message().c_str());
        }
    }
}


/*
    rask::stat
*/


rask::stat::stat(int64_t s, fostlib::timestamp ts)
: size(s), modified(ts) {
}


rask::stat::stat(const fostlib::json &j)
: size(fostlib::coerce<int64_t>(j["size"]["bytes"])),
    modified(fostlib::coerce<fostlib::timestamp>(j["modified"]))
{}


fostlib::json fostlib::coercer<fostlib::json, rask::stat>::coerce(const rask::stat &s) {
    fostlib::json j;
    fostlib::insert(j, "size", "bytes", s.size);
    fostlib::insert(j, "modified", s.modified);
    return j;
}


/*
    rask::file_stat
*/


rask::stat rask::file_stat(const boost::filesystem::path &filename) {
    struct ::stat status;
    const auto stat_call = syscall(
        [&filename, &status]() {
            return ::stat(filename.c_str(), &status);
        });
    if ( stat_call == 0 ) {
        return rask::stat{
            status.st_size,
            fostlib::timestamp(
                    boost::posix_time::from_time_t(status.st_mtim.tv_sec)) +
                boost::posix_time::microseconds(status.st_mtim.tv_nsec / 1'000)
        };
    } else {
        fostlib::log::error(c_fost_rask)
            ("", "file_stat")
            ("filename", filename);
        throw fostlib::exceptions::not_implemented(
            "Error when fetching stat for file");
    }
}


/*
    rask::file::data
*/


struct rask::file::data::impl {
    std::size_t size;
    boost::iostreams::mapped_file_source file;

    /// We can't memory map a zero byte file, so we only open the file
    impl(std::size_t s, const boost::filesystem::path &p)
    : size(s) {
        if ( size ) file.open(p.string());
    }

    fostlib::const_memory_block data(std::size_t offset, std::size_t bytes) {
        if ( !size ) {
            throw fostlib::exceptions::null("Memory mapped file is empty");
        } else if ( offset >= size ) {
            throw fostlib::exceptions::unexpected_eof(
                "Requesting data past the end of the file");
        }
        const unsigned char *begin =
            reinterpret_cast< const unsigned char * >(file.data());
        const unsigned char *starts = begin + offset;
        const unsigned char *ends = std::min(begin + size, starts + bytes);
        return std::make_pair(starts, ends);
    }
};


rask::file::data::data(boost::filesystem::path p)
: pimpl(std::make_shared<impl>(boost::filesystem::file_size(p), p)),
    location(std::move(p))
{
}


rask::file::const_block_iterator rask::file::data::begin() const {
    return const_block_iterator(pimpl);
}


rask::file::const_block_iterator rask::file::data::end() const {
    return const_block_iterator();
}


/*
    rask::file::const_block_iterator
*/


rask::file::const_block_iterator::const_block_iterator(
    std::shared_ptr<data::impl> fptr
) : file(fptr && fptr->size ? fptr : nullptr), m_offset(0) {
}


rask::file::const_block_iterator &rask::file::const_block_iterator::operator ++ () {
    if ( !file )
        throw fostlib::exceptions::unexpected_eof(
            "Can't increment an end rask::file::const_block_iterator");
    m_offset += file_hash_block_size;
    if ( m_offset >= file->size ) {
        file.reset();
        m_offset = 0;
    }
    return *this;
}


rask::file::const_block_iterator::value_type
        rask::file::const_block_iterator::operator * () const {
    if ( !file )
        throw fostlib::exceptions::unexpected_eof(
            "Can't dereference an end rask::file::const_block_iterator");
    return std::make_pair(m_offset,
        file->data(m_offset, file_hash_block_size));
}


bool rask::file::const_block_iterator::operator == (
    const rask::file::const_block_iterator &right
) const {
    return file == right.file && m_offset == right.m_offset;
}

