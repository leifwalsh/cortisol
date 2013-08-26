/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include "mongo/client/dbclient.h"

#include "collection.h"

namespace cortisol {

namespace po = boost::program_options;

class UpdateRunner : public CollectionRunner {
  public:
    UpdateRunner(const Options &opts, const string &ns, size_t id, timestamp_t t0) : CollectionRunner(opts, ns, id, t0) {}
    void step(mongo::DBClientBase &conn);

    virtual const string &name() const {
        static const string n = "update";
        return n;
    }

    // config
    static size_t threads;
    static po::options_description options_description() {
        po::options_description desc("Update Thread");
        desc.add_options()
                ("update.threads", po::value(&threads)->default_value(threads), "# of threads.")
                ;
        return desc;
    }
};

class PointQueryRunner : public CollectionRunner {
  public:
    PointQueryRunner(const Options &opts, const string &ns, size_t id, timestamp_t t0) : CollectionRunner(opts, ns, id, t0) {}
    void step(mongo::DBClientBase &conn);

    virtual const string &name() const {
        static const string n = "ptquery";
        return n;
    }

    // config
    static size_t threads;
    static po::options_description options_description() {
        po::options_description desc("Point Query Thread");
        desc.add_options()
                ("point_query.threads", po::value(&threads)->default_value(threads), "# of threads.")
                ;
        return desc;
    }
};

class RangeQueryRunner : public CollectionRunner {
  public:
    RangeQueryRunner(const Options &opts, const string &ns, size_t id, timestamp_t t0) : CollectionRunner(opts, ns, id, t0) {}
    void step(mongo::DBClientBase &conn);

    virtual const string &name() const {
        static const string n = "rgquery";
        return n;
    }

    // stats
    static long long bytes;

    // config
    static size_t threads;
    static size_t stride;
    static po::options_description options_description() {
        po::options_description desc("Range Query Thread");
        desc.add_options()
                ("range_query.threads", po::value(&threads)->default_value(threads), "# of threads.")
                ("range_query.stride",  po::value(&stride)->default_value(stride),   "# of docs to query at once.")
                ;
        return desc;
    }
};

} // namespace cortisol
