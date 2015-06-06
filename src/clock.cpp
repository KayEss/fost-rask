/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <rask/clock.hpp>
#include <rask/configuration.hpp>
#include <rask/server.hpp>

#include <beanbag/beanbag>
#include <fost/push_back>


rask::tick::tick(int64_t t)
: time(t), server(server_identity()), reserved(0) {
}


rask::tick::tick(int64_t t, uint32_t s)
: time(t), server(s), reserved(0) {
}


rask::tick::tick(const fostlib::json &j)
: time(fostlib::coerce<int64_t>(j[0])),
        server(fostlib::coerce<uint32_t>(j[1])),
        reserved(0) {
}


std::pair<rask::tick, fostlib::nullable<fostlib::string>> rask::tick::now() {
    if ( !c_server_db.value().isnull() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_server_db.value()["database"]));
        fostlib::jsondb::local server(*dbp);
        auto hash(fostlib::coerce<fostlib::nullable<fostlib::string>>(server["hash"]));
        fostlib::jcursor location("time");
        if ( server.has_key(location) ) {
            return std::make_pair(tick(server[location]), hash);
        } else {
            return std::make_pair(tick(0u), hash);
        }
    } else {
        return std::make_pair(tick(0u), fostlib::null);
    }
}


rask::tick rask::tick::next() {
    if ( !c_server_db.value().isnull() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_server_db.value()["database"]));
        fostlib::jsondb::local server(*dbp);
        int64_t time;
        server.transformation([&time](fostlib::json &db) {
            /*
                By storing time without the server identity we allow the identity
                to be changed and we still get good times afterwards.
            */
            fostlib::jcursor location("time");
            if ( db.has_key(location) ) {
                time = fostlib::coerce<int64_t>(db[location][0]) + 1;
                location.replace(db, tick(time));
            } else {
                time = 1;
                location.insert(db, tick(time));
            }
        });
        server.commit();
        return tick(time);
    } else {
        throw fostlib::exceptions::not_implemented(
            "tick::next() when there is no server database");
    }
}


rask::tick rask::tick::overheard(int64_t t, uint32_t s) {
    rask::tick heard(t, s);
    if ( !c_server_db.value().isnull() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_server_db.value()["database"]));
        fostlib::jsondb::local server(*dbp);
        server.transformation([heard](fostlib::json &db) {
            fostlib::jcursor location("time");
            if ( db.has_key(location) ) {
                rask::tick mytime(db[location]);
                if ( mytime < heard ) {
                    location.replace(db, heard);
                }
            } else {
                location.insert(db, heard);
            }
        });
        server.commit();
    }
    return heard;
}


fostlib::json fostlib::coercer<fostlib::json, rask::tick>::coerce(const rask::tick &t) {
    fostlib::json j;
    fostlib::push_back(j, t.time);
    fostlib::push_back(j, t.server);
    return j;
}


fostlib::digester &fostlib::operator << (fostlib::digester &d, const rask::tick &t) {
    return d << fostlib::const_memory_block{&t, &t + 1};
}

