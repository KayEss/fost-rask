/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/base32.hpp>
#include <rask/sweep.hpp>
#include <fost/main>


using namespace fostlib;


FSL_MAIN(
    L"file-hashes",
    L"File hashes\nCopyright 2015, Proteus Tech Co. Ltd."
)( fostlib::ostream &out, fostlib::arguments &args ) {
    if ( args.size() < 2 ) {
        out << "Specify one or more files to hash" << std::endl;
        return 1;
    }
    for ( auto fileindex = 1; fileindex < args.size(); ++fileindex ) {
        auto filename(coerce<boost::filesystem::path>(args[fileindex].value()));
        out << filename << std::endl;
        rask::const_file_block_iterator end;
        for ( rask::const_file_block_iterator block(filename); block != end; ++block ) {
            std::cout << coerce<rask::base32_string>(*block) << std::endl;
        }
    }

    return 0;
}
