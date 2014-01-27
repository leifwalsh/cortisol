/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include <stdlib.h>

#include <algorithm>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include "mongo/client/dbclient.h"
#include "mongo/util/mongoutils/str.h"

#include "cortisol.h"
#include "runner.h"
#include "setup.h"
#include "util/counter.h"
#include "util/queue.h"
#include "util/timing.h"

namespace cortisol {

namespace tokumx {

namespace po = boost::program_options;

static void gen_field(size_t i, std::stringstream &ss) {
    if (i < 26) {
        ss << (char) ('a' + i);
    } else {
        gen_field(i / 26, ss);
        gen_field(i % 26, ss);
    }
}

static std::string field(size_t i) {
    static std::vector<std::string> fields;
    if (fields.size() <= i) {
        static std::mutex fields_mutex;
        std::lock_guard<std::mutex> lk(fields_mutex);
        while (fields.size() <= i) {
            std::stringstream ss;
            gen_field(fields.size(), ss);
            fields.push_back(ss.str());
        }
    }
    return fields[i];
}

using mongo::BSONObj;
using mongo::BSONObjBuilder;

struct DBConfig {
    static std::string host;
    static int port;
    static std::string db;
    static size_t collections;
    static size_t documents;
    static size_t fields;
    static size_t padding;
    static double compressibility;

    static void add_options(po::options_description &desc) {
        desc.add_options()
                ("tokumx.host", po::value(&host)->default_value("localhost"), "tokumx server hostname")
                ("tokumx.port", po::value(&port)->default_value(27017), "tokumx server port")
                ("tokumx.db", po::value(&db)->default_value("cortisol"), "db to use")
                ("tokumx.collections", po::value(&collections)->default_value(4), "number of collections to use")
                ("tokumx.documents", po::value(&documents)->default_value(100000), "number of documents (per collection)")
                ("tokumx.fields", po::value(&fields)->default_value(2), "number of fields per document")
                ("tokumx.padding", po::value(&padding)->default_value(100), "padding per document")
                ("tokumx.compressibility", po::value(&compressibility)->default_value(0.5), "compressibility factor")
                ;
    }

    static std::string ns(size_t idx) {
        return mongoutils::str::stream() << db << (idx % collections) << ".coll";
    }
};
std::string DBConfig::host;
int DBConfig::port;
std::string DBConfig::db;
size_t DBConfig::collections;
size_t DBConfig::documents;
size_t DBConfig::fields;
size_t DBConfig::padding;
double DBConfig::compressibility;

static void _random_obj(BSONObjBuilder &b, bool with_id, bool with_padding) {
    if (with_id) {
        b << mongo::GENOID;
    }
    for (size_t i = 0; i < DBConfig::fields; ++i) {
        b.appendIntOrLL(field(i), random() % DBConfig::documents);
    }
    if (with_padding) {
        const size_t zero_bytes = DBConfig::padding * DBConfig::compressibility;
        std::unique_ptr<char[]> buf(new char[DBConfig::padding]);
        std::fill(&buf[0], &buf[zero_bytes], 0);
        std::generate(&buf[zero_bytes], &buf[DBConfig::padding], []() { return (char) (random() & 0xff); });
        b.appendBinData("padding", DBConfig::padding, mongo::BinDataGeneral, buf.get());
    }
}

class Connection {
    std::unique_ptr<mongo::ScopedDbConnection> _conn;
  public:
    Connection()
            : _conn(mongo::ScopedDbConnection::getScopedDbConnection(DBConfig::host, DBConfig::port)) {}
    ~Connection() {
        _conn->done();
    }
    mongo::DBClientBase &client() { return _conn->conn(); }
};

class TokumxSetup : public core::Setup {
    size_t _loader_batchsize;
    bool _keep_database;
    bool _create;

  public:
    TokumxSetup() : core::Setup("TokuMX") {}

    void add_options(po::options_description &desc) {
        DBConfig::add_options(desc);
        desc.add_options()
                ("tokumx.loader-batchsize", po::value(&_loader_batchsize)->default_value(1000), "batch size for the loader")
                ("tokumx.keep-database", po::value(&_keep_database)->default_value(false), "keep the old database intact")
                ("tokumx.create", po::value(&_create)->default_value(true), "seed the database with freshly loaded data")
                ;
    }

    typedef util::Queue<std::unique_ptr<std::vector<BSONObj> > > BatchQueue;

    class Generator : public core::Runner {
        const std::string _name;
        BatchQueue _queue;
        const size_t _batchsize;
        size_t _generated;

        static BSONObj random_obj() {
            BSONObjBuilder b;
            _random_obj(b, true, true);
            return b.obj();
        }

      public:
        Generator(size_t batchsize, size_t idx)
                : core::Runner(),
                  _name(mongoutils::str::stream() << "gen" << idx),
                  _queue(5),
                  _batchsize(batchsize),
                  _generated(0) {}

        const std::string &name() const { return _name; }

        void step() {
            size_t this_batch = std::min(_batchsize, DBConfig::documents - _generated);
            std::unique_ptr<std::vector<BSONObj> > vec(new std::vector<BSONObj>(this_batch));
            std::generate(vec->begin(), vec->end(), &Generator::random_obj);
            _queue.push(std::move(vec));
            _generated += this_batch;
            if (_generated == DBConfig::documents) {
                stop();
            }
        }

        BatchQueue& queue() {
            return _queue;
        }

        size_t report() { return 0; }
        void total() {}
    };

    class Loader : public core::Runner {
        const std::string _name;
        BatchQueue &_queue;
        const std::string _collname;
        util::counter<size_t> _loaded;
        const size_t _idx;

        BSONObj index_spec() {
            size_t n = _idx;
            BSONObjBuilder b;
            // Always need at least one field, so keep this out of the loop.
            b.append("a", (n&1) ? -1 : 1);
            n >>= 1;
            for (size_t idx = 1; n > 0; ++idx, n >>= 1) {
                b.append(field(idx), (n&1) ? -1 : 1);
            }
            return b.obj();
        }

        std::string index_name() {
            size_t n = _idx;
            std::stringstream ss;
            // Always need at least one field, so keep this out of the loop.
            ss << "a_" << ((n&1) ? -1 : 1);
            n >>= 1;
            for (size_t idx = 1; n > 0; ++idx, n >>= 1) {
                ss << "_" << field(idx) << "_" << ((n&1) ? -1 : 1);
            }
            return ss.str();
        }

      public:
        Loader(BatchQueue &queue, size_t idx)
                : core::Runner(),
                  _name(mongoutils::str::stream() << "load" << idx),
                  _queue(queue),
                  _collname(DBConfig::ns(idx)),
                  _loaded(now()),
                  _idx(idx) {}

        const std::string &name() const { return _name; }

        void step() {
            std::unique_ptr<std::vector<BSONObj> > batch = std::move(_queue.front());
            _queue.pop();
            {
                Connection c;
                try {
                    c.client().insert(_collname, *batch);
                    if (verbose) {
                        std::cout << "inserted " << batch->size() << " elements" << std::endl;
                    }
                    _loaded += batch->size();
                } catch (mongo::DBException &e) {
                    std::cerr << "error " << e.getCode() << " inserting batch: " << e.what() << std::endl;
                    stop();
                }
            }
            if (_loaded == DBConfig::documents) {
                _queue.drain();
                BSONObjBuilder b;
                b << "ns" << _collname
                  << "key" << index_spec()
                  << "name" << index_name();
                Connection c;
                BSONObj spec = b.done();
                c.client().insert(mongoutils::str::stream() << DBConfig::db << _idx << ".system.indexes", spec);
                c.client().getLastError();
                stop();
            }
        }

        size_t report() {
            std::cout << util::out::pad(10) << name()
                      << util::out::ofs << _loaded.report(now())
                      << std::endl;
            return 1;
        }

        void total() {
            std::cout << util::out::pad(10) << name()
                      << util::out::ofs << _loaded.total(now())
                      << std::endl;
        }
    };

    void setup() {
        if (!_keep_database) {
            Connection c;
            for (size_t i = 0; i < DBConfig::collections; ++i) {
                c.client().dropDatabase(mongoutils::str::stream() << DBConfig::db << i);
            }
        }
    }

    void generate_setup_runners(std::vector<std::unique_ptr<core::Runner> > &runners) {
        if (_create) {
            for (size_t i = 0; i < DBConfig::collections; ++i) {
                std::unique_ptr<Generator> generator(new Generator(_loader_batchsize, i));
                std::unique_ptr<Loader> loader(new Loader(generator->queue(), i));
                runners.push_back(std::move(generator));
                runners.push_back(std::move(loader));
            }
        }
    }
} tokumxSetup;

class TokumxRunner : public core::Runner {
    size_t _idx;
    std::string _ns;

  protected:
    const char *ns() const {
        return _ns.c_str();
    }

  public:
    TokumxRunner(size_t idx)
            : core::Runner(),
              _idx(idx),
              _ns(DBConfig::ns(idx)) {}
    virtual ~TokumxRunner() {}
};

class TokumxUpdate : public TokumxRunner {
    std::string _name;

    static BSONObj random_a() {
        BSONObjBuilder b;
        b.appendIntOrLL("a", random() % DBConfig::documents);
        return b.obj();
    }

  public:
    TokumxUpdate(size_t idx)
            : TokumxRunner(idx),
              _name(mongoutils::str::stream() << "update" << idx) {}

    virtual const std::string &name() const { return _name; }

    virtual void step() {
        BSONObj spec = random_a();
        BSONObjBuilder b;
        BSONObjBuilder incb(b.subobjStart("$inc"));
        _random_obj(incb, false, false);
        incb.doneFast();

        Connection c;
        BSONObj obj = b.done();
        c.client().update(ns(), spec, obj, false, false);
    }
};

class TokumxUpdateFactory : public core::NRunnerFactory {
  protected:
    virtual std::unique_ptr<core::Runner> make(size_t i) const {
        std::unique_ptr<core::Runner> p(new TokumxUpdate(i));
        return p;
    }
    virtual void add_options(po::options_description &desc) {}
  public:
    TokumxUpdateFactory() : NRunnerFactory("Update", "updates") {}
} tokumxUpdateFactory;

} // namespace tokumx

} // namespace cortisol
