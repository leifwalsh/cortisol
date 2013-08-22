/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "mongo/client/dbclient.h"
#include "mongo/client/remote_loader.h"

#include "alarm.h"
#include "collection.h"
#include "cortisol.h"
#include "counter.h"
#include "timing.h"
#include "queue.h"
#include "words.h"

namespace cortisol {

using std::auto_ptr;
using std::cout;
using std::endl;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::vector;

using mongo::BSONObj;
using mongo::BSONObjBuilder;
using mongo::RemoteLoader;


string server = "localhost";

Wordlist wl("/usr/share/dict/cracklib-small");

static void gen_field(size_t i, stringstream &ss) {
    if (i < 26) {
        ss << (char) ('a' + i);
    } else {
        gen_field(i / 26, ss);
        gen_field(i % 26, ss);
    }
}

static string field(size_t i) {
    static vector<string> fields;
    while (fields.size() <= i) {
        stringstream ss;
        gen_field(fields.size(), ss);
        fields.push_back(ss.str());
    }
    return fields[i];
}

/** @return the spec for the nth index, defined by interpreting the bits in n. */
static BSONObj index_spec(size_t n) {
    BSONObjBuilder b;
    // Always need at least one field, so keep this out of the loop.
    b.append("a", (n&1) ? -1 : 1);
    n >>= 1;
    for (size_t idx = 1; n > 0; ++idx, n >>= 1) {
        b.append(field(idx), (n&1) ? -1 : 1);
    }
    return b.obj();
}

static string index_name(size_t n) {
    stringstream ss;
    // Always need at least one field, so keep this out of the loop.
    ss << "a_" << ((n&1) ? -1 : 1);
    n >>= 1;
    for (size_t idx = 1; n > 0; ++idx, n >>= 1) {
        ss << "_" << field(idx) << "_" << ((n&1) ? -1 : 1);
    }
    return ss.str();
}

/** @return a random document */
static void _random_obj(BSONObjBuilder &b, const Options &opts, bool with_id, bool with_padding) {
    if (with_id) {
        b << mongo::GENOID;
    }
    for (size_t i = 0; i < opts.fields; ++i) {
        b.appendIntOrLL(field(i), random() % opts.documents);
    }
    if (with_padding) {
        //b << "str" << wl.randstr(opts.padding);
    }
}

static inline BSONObj random_obj(const Options &opts) {
    BSONObjBuilder b;
    _random_obj(b, opts, true, true);
    return b.obj();
}

BSONObj random_a(const Options &opts) {
    BSONObjBuilder b;
    b.appendIntOrLL("a", random() % opts.documents);
    return b.obj();
}

void Collection::drop() {
    conn().dropCollection(ns());
}

vector<BSONObj> Collection::index_specs() const {
    vector<BSONObj> vec;
    size_t i = 0;
    std::generate_n(std::back_inserter(vec), _opts.indexes,
                    [this, &i]() {
                        BSONObjBuilder b;
                        b << "ns" << ns()
                          << "key" << index_spec(i)
                          << "name" << index_name(i);
                        if (is_tokumx()) {
                            b << "compression" << "zlib"
                              << "readPageSize" << (128 << 10);
                            if (_opts.clustering) {
                                b << "clustering" << true;
                            }
                        }
                        ++i;
                        return b.obj();
                    });
    return vec;
}

void Collection::create_options(BSONObjBuilder &b) const {
    if (is_tokumx()) {
        b.append("compression", "zlib");
        b.append("readPageSize", 128 << 10);
    }
}

void Collection::ensure_indexes() {
    {
        BSONObj err;
        BSONObjBuilder b;
        b << "create" << collname();
        create_options(b);
        conn().runCommand(dbname(), b.done(), err);
    }

    string system_indexes(dbname() + ".system.indexes");
    vector<BSONObj> indexes = index_specs();
    std::for_each(indexes.begin(), indexes.end(), [this, system_indexes](const BSONObj &spec) {
            conn().insert(system_indexes, spec);
        });
}

void Collection::fill() {
    static std::mutex output_mutex;

    unique_ptr<RemoteLoader> loader;
    if (_opts.loader) {
        BSONObjBuilder options;
        create_options(options);
        loader.reset(new RemoteLoader(conn(), dbname(), collname(), index_specs(), options.done()));
    } else {
        ensure_indexes();
    }

    Queue<shared_ptr<vector<BSONObj> > > objs_queue(100);

    try {
        static const size_t batch = 1<<17;
        std::thread producer([&objs_queue, this]() {
                try {
                    for (size_t i = 0; i < _opts.documents; interrupter.check_for_interrupt()) {
                        size_t this_batch = std::min(batch, _opts.documents - i);
                        shared_ptr<vector<BSONObj> > vec(new vector<BSONObj>(this_batch));
                        std::generate(vec->begin(), vec->end(), std::bind(random_obj, _opts));
                        objs_queue.push(vec);
                        i += this_batch;
                    }
                } catch (interrupt_exception) {
                }
            });
        producer.detach();

        timestamp_t t0 = now();
        counter<size_t> i(t0);
        for (; i < _opts.documents; interrupter.check_for_interrupt()) {
            size_t this_batch = std::min(batch, _opts.documents - i);
            shared_ptr<vector<BSONObj> > objs = objs_queue.front();
            objs_queue.pop();
            conn().insert(ns(), *objs);
            i += this_batch;

            {
                std::lock_guard<std::mutex> lk(output_mutex);
                cout << ns() << ofs
                     << std::setiosflags(std::ios_base::right) << std::setw(10) << "fill" << ofs
                     << i.report(now()) << ors;
            }
        }

        if (loader) {
            interrupter.check_for_interrupt();
            loader->commit();
            {
                std::lock_guard<std::mutex> lk(output_mutex);
                cout << ns() << ofs
                     << std::setiosflags(std::ios_base::right) << std::setw(10) << "fill" << ofs
                     << i.report(now()) << ors;
            }
        }
    } catch (interrupt_exception) {
        objs_queue.drain();
    }
}


size_t UpdateRunner::threads = 0;
void UpdateRunner::step(mongo::DBClientBase &conn) {
    BSONObj spec = random_a(_opts);
    BSONObjBuilder b;
    BSONObjBuilder incb(b.subobjStart("$inc"));
    _random_obj(incb, _opts, false, false);
    incb.doneFast();

    {
        alarm a;
        conn.update(ns(), spec, b.done());
    }
}

size_t PointQueryRunner::threads = 0;
void PointQueryRunner::step(mongo::DBClientBase &conn) {
    BSONObj spec = random_a(_opts);
    {
        alarm a;
        auto_ptr<mongo::DBClientCursor> c = conn.query(ns(), spec);
        while (c->more()) {
            c->next();
        }
    }
}

long long RangeQueryRunner::bytes = 0;
size_t RangeQueryRunner::threads = 0;
size_t RangeQueryRunner::stride = 0;
void RangeQueryRunner::step(mongo::DBClientBase &conn) {
    long long x = random() % (_opts.documents - stride);
    long long y = x + stride;
    {
        alarm a;
        auto_ptr<mongo::DBClientCursor> c = conn.query(ns(), QUERY( "a" << mongo::GTE << x << mongo::LT << y ));
        while (c->more()) {
            bytes += c->next().objsize();
        }
    }
}

} // namespace cortisol
