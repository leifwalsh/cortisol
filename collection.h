/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>

#include "mongo/client/dbclient.h"

#include "counter.h"
#include "options.h"
#include "output.h"
#include "thread.h"

namespace cortisol {

using std::cerr;
using std::endl;
using std::unique_ptr;
using std::string;
using std::stringstream;
using std::vector;

using out::ofs;
using out::ors;

using mongo::BSONObj;
using mongo::BSONObjBuilder;

extern thread_interrupter interrupter;

class ConnectionInfo {
    string _ns;

  protected:
    Options _opts;

    const string &ns() const {
        return _ns;
    }
    string dbname() const {
        stringstream ss;
        const char *n = _ns.c_str();
        const char *dot = strchr(n, '.');
        ss.write(n, dot - n);
        return ss.str();
    }
    string collname() const {
        stringstream ss;
        const char *n = _ns.c_str();
        const char *dot = strchr(n, '.');
        ss.write(dot + 1, _ns.size() - ((dot + 1) - n));
        return ss.str();
    }

  public:
    ConnectionInfo(const Options &opts, const string &ns) : _ns(ns), _opts(opts) {}
};

class Collection : public ConnectionInfo {
    unique_ptr<mongo::ScopedDbConnection> _c;
    bool _is_tokumx;

    vector<BSONObj> index_specs() const;
    void create_options(BSONObjBuilder &b) const;
    void ensure_indexes();

    mongo::DBClientBase &conn() const {
        return _c->conn();
    }
    bool is_tokumx() const {
        return _is_tokumx;
    }
  public:
    Collection(const Options &opts, const string &ns) : ConnectionInfo(opts, ns), _c(mongo::ScopedDbConnection::getScopedDbConnection(opts.host)) {
        BSONObj res;
        bool ok = conn().simpleCommand("admin", &res, "buildInfo");
        assert(ok);
        _is_tokumx = res["tokumxVersion"].ok();
    }
    ~Collection() {
        if (_c) {
            _c->done();
        }
    }
    void drop();
    void fill();

    // config
    static size_t collections;
    static size_t indexes;
    static bool clustering;
    static size_t fields;
    static size_t documents;
    static size_t padding;
    static double compressibility;
    static po::options_description options_description();

    Collection(Collection &&o) = default;
    Collection &operator=(Collection &&o) = default;
};

class CollectionRunner : public ConnectionInfo {
    bool _running;
    size_t _id;
    counter<size_t> _steps;

  public:
    CollectionRunner(const Options &opts, const string &ns, size_t id, timestamp_t t0) : ConnectionInfo(opts, ns), _running(true), _id(id), _steps(t0) {}

    class UnimplementedException : public std::exception {};
    virtual void step(mongo::DBClientBase &) {
        throw UnimplementedException();
    }

    virtual const string &name() const {
        static const string n = "unknown";
        return n;
    }

    void operator()() {
        while (_running) {
            unique_ptr<mongo::ScopedDbConnection> c(mongo::ScopedDbConnection::getScopedDbConnection(_opts.host));
            try {
                interrupter.check_for_interrupt();
                step(c->conn());
                _steps++;
            } catch (interrupt_exception &e) {
                stop();
            } catch (UnimplementedException) {
                cerr << "unimplemented step()" << endl;
                stop();
            } catch (std::exception &e) {
                cerr << "caught exception " << e.what() << endl;
            }
            c->done();
        }
    }

    template<class ostream_type>
    static void header(ostream_type &os) {
        os << "# " << out::pad(16) << "ns" << ofs
           << out::pad(10) << "type" << ofs
           << out::pad(4) << "id" << ofs
           << counter<size_t>::header() << ors;
    }

    template<class ostream_type>
    void report(ostream_type &os, timestamp_t ti) {
        static std::mutex _m;
        std::lock_guard<std::mutex> lk(_m);
        os << out::pad(18) << ns() << ofs
           << out::pad(10) << name() << ofs
           << out::pad(4) << _id << ofs
           << _steps.report(ti) << ors;
    }

    template<class ostream_type>
    void total(ostream_type &os, timestamp_t ti) {
        os << out::pad(18) << ns() << ofs
           << out::pad(10) << name() << ofs
           << out::pad(4) << _id << ofs
           << _steps.total(ti) << ors;
    }

    void stop() {
        _running = false;
    }
};

} // namespace cortisol
