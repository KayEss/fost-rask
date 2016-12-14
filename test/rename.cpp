/*
    Copyright 2016, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


/// We're seeing errors when beanbags are being saved. The tmpfile to
/// beanbag rename is failing because very occasionally the tmp file no
/// longer exists.


#include <fost/file>
#include <fost/main>

#include <thread>


const std::size_t c_threads = 16;
const std::size_t c_trials = 1 << 22;
const std::size_t c_mask = 0x3ff;


auto procedure = [](auto &out, auto id) {
        return [&out, id]() {
                const boost::filesystem::path tmp(std::to_string(id) + ".tmp");
                const boost::filesystem::path txt(std::to_string(id) + ".txt");
                boost::system::error_code error;
                for ( auto count = 0; count < c_trials; ++count ) {
                    fostlib::utf::save_file(tmp, "File content\n");
                    boost::filesystem::rename(tmp, txt, error);
                    if ( error ) {
                        std::cerr << error << " in thread " << id << std::endl;
                        std::exit(1);
                    }
                    if ( id == 0 && (count & c_mask) == c_mask ) {
                        out << count << std::endl;
                    }
                }
            };
    };


FSL_MAIN("rename", "Rename test")(fostlib::ostream &out, fostlib::arguments &args) {
    std::vector<std::thread> threads;
    for ( auto thread = 1; thread < c_threads; ++thread ) {
        threads.emplace_back(procedure(out, thread));
    }
    /// Also run in main thread
    procedure(out, 0)();

    for ( auto &thread : threads ) {
        thread.join();
    }
    return 0;
}
