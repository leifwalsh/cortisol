/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <string>

#include <boost/program_options.hpp>

namespace cortisol {

using std::string;

namespace po = boost::program_options;

class Options {
    Options() {}
  public:
    bool create;
    bool stress;
    bool keep_database;
    bool loader;
    string host;

    size_t collections;
    size_t indexes;
    bool clustering;
    size_t fields;
    size_t documents;
    size_t padding;
    double compressibility;

    int seconds;

    string ofs;
    string ors;

    static Options default_options();
    explicit Options(const po::variables_map &vm);
    po::options_description options_description() const;
};

bool parse_cmdline(int argc, const char *argv[], Options &opts);

} // namespace cortisol
