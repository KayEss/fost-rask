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


rask::tick rask::tick::now() {
    if ( !c_server_db.value().isnull() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_server_db.value()["database"]));
        fostlib::jsondb::local server(*dbp);
        fostlib::jcursor location("time");
        if ( server.has_key(location) ) {
            return tick(fostlib::coerce<int64_t>(server[location]));
        } else {
            return tick(0u);
        }
    } else {
        throw fostlib::exceptions::not_implemented(
            "tick::now() when there is no server database");
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
                time = fostlib::coerce<int64_t>(db[location]) + 1;
                location.replace(db, time);
            } else {
                time = 1;
                location.insert(db, time);
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
                rask::tick mytime(fostlib::coerce<int64_t>(db[location]));
                if ( mytime < heard ) {
                    location.replace(db, fostlib::coerce<fostlib::json>(heard));
                }
            } else {
                location.insert(db, heard.time);
            }
        });
        server.commit();
    }
    return heard;
}


fostlib::json fostlib::coercer<fostlib::json, rask::tick>::coerce(rask::tick t) {
    fostlib::json j;
    fostlib::push_back(j, t.time);
    fostlib::push_back(j, t.server);
    return j;
}


fostlib::digester &fostlib::operator << (digester &d, rask::tick t) {
    return d << const_memory_block(&t, &t + sizeof(rask::tick));
}
