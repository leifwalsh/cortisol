/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include <boost/program_options.hpp>

#include "cortisol.h"
#include "options.h"

namespace cortisol {

using std::cout;
using std::cerr;
using std::endl;
using std::ifstream;
using std::string;

namespace po = boost::program_options;

Options Options::default_options() {
    Options opts;
    opts.create = true;
    opts.stress = true;
    opts.keep_database = false;
    opts.loader = true;
    opts.host = "127.0.0.1";
    opts.collections = 1;
    opts.indexes = 3;
    opts.clustering = false;
    opts.fields = 2;
    opts.documents = 1<<20;
    opts.padding = 100;
    opts.compressibility = 0.25;
    opts.seconds = 60;
    opts.pad_output = true;
    opts.ofs = "\t";
    opts.ors = "\n";
    opts.output_period = 1.0;
    opts.header_frequency = 20;
    return opts;
}

Options::Options(const po::variables_map &vm)
        : create          (vm["create"].as<bool>()),
          stress          (vm["stress"].as<bool>()),
          keep_database   (vm["keep-database"].as<bool>()),
          loader          (vm["loader"].as<bool>()),
          host            (vm["host"].as<string>()),
          collections     (vm["collections"].as<size_t>()),
          indexes         (vm["indexes"].as<size_t>()),
          clustering      (vm["clustering"].as<bool>()),
          fields          (vm["fields"].as<size_t>()),
          documents       (vm["documents"].as<size_t>()),
          padding         (vm["padding"].as<size_t>()),
          compressibility (vm["compressibility"].as<double>()),
          seconds         (vm["seconds"].as<int>()),
          pad_output      (vm["pad-output"].as<bool>()),
          ofs             (vm["ofs"].as<string>()),
          ors             (vm["ors"].as<string>()),
          output_period   (vm["output-period"].as<double>()),
          header_frequency(vm["header-frequency"].as<int>())
{}

po::options_description Options::options_description() const {
    po::options_description conn_options("Connection");
    conn_options.add_options()
            ("host", po::value<string>()->default_value(host), "Host to connect to.  Can also be a replica set with full \"mongodb://host1,host2,host3/?replicaSet=rsName\" syntax.")
            ;
        

    po::options_description exec_options("Execution");
    exec_options.add_options()
            ("create",        po::value<bool>()->default_value(create),        "Create and fill the collections. (skip create: --create=off)")
            ("stress",        po::value<bool>()->default_value(stress),        "Stress an existing set of collections. (skip stress: --stress=off)")
            ("keep-database", po::bool_switch(), "Don't drop the existing database before running.")
            ("loader",        po::value<bool>()->default_value(loader),        "Use the bulk loader to load collections.")
            ;

    po::options_description coll_options("Collection");
    coll_options.add_options()
            ("collections",     po::value<size_t>()->default_value(collections),     "# of collections.")
            ("indexes",         po::value<size_t>()->default_value(indexes),         "# of indexes per collection.")
            ("clustering",      po::value<bool>()->default_value(clustering),         "Whether to cluster the secondary indexes.")
            ("fields",          po::value<size_t>()->default_value(fields),          "# of fields in each document.")
            ("documents",       po::value<size_t>()->default_value(documents),       "# of documents per collection.")
            ("padding",         po::value<size_t>()->default_value(padding),         "Additional bytes of padding to fill documents with.")
            ("compressibility", po::value<double>()->default_value(compressibility), "Compressibility of padding (between 0 and 1: 0 means purely random, 1 means fill with zeroes).")
            ("seconds",         po::value<int>()->default_value(seconds),            "Time to run for.")
            ;

    po::options_description disp_options("Display");
    disp_options.add_options()
            ("pad-output",              po::value<bool>()->default_value(pad_output),      "Pad output fields into columns.")
            ("ofs",                     po::value<string>()->default_value(ofs),           "Output field separator.")
            ("ors",                     po::value<string>()->default_value(ors),           "Output record separator.")
            ("output-period",           po::value<double>()->default_value(output_period), "Seconds between output.")
            ("header-frequency",        po::value<int>()->default_value(header_frequency), "Lines between printing headers.")
            ;

    po::options_description all_options("General");
    all_options
            .add(conn_options)
            .add(exec_options)
            .add(coll_options)
            .add(disp_options)
            .add(UpdateRunner::options_description())
            .add(PointQueryRunner::options_description())
            .add(RangeQueryRunner::options_description())
            ;
    return all_options;
}

bool parse_cmdline(int argc, const char *argv[], Options &opts) {
    po::options_description visible_options("Options");
    visible_options.add_options()
            ("help,h", "Get help.")
            ("verbose,v", "More verbose output.")
            ;

    visible_options.add(opts.options_description());

    po::options_description all_options;
    all_options.add_options()
            ("response-file", po::value<vector<string> >(), "Config file.")
            ;

    all_options.add(visible_options);

    try {
        po::variables_map cli_vm;
        auto cli_parsed = po::parse_command_line(argc, argv, all_options, 0,
                                                 [](const string &s) -> std::pair<string, string> {
                                                     if (s[0] == '@') {
                                                         return std::make_pair(string("response-file"), s.substr(1));
                                                     } else {
                                                         return std::pair<string, string>();
                                                     }
                                                 });
        po::store(cli_parsed, cli_vm);

        po::variables_map vm;
        if (cli_vm.count("response-file")) {
            const vector<string> files = cli_vm["response-file"].as<vector<string> >();
            for (vector<string>::const_iterator it = files.begin(); it != files.end(); ++it) {
                ifstream ifs(it->c_str());
                if (ifs.good()) {
                    po::store(po::parse_config_file(ifs, all_options), vm);
                }
            }
        }
        po::store(cli_parsed, vm);

        if (vm.count("help")) {
            cout << "cortisol:" << endl
                 << visible_options << endl;
            return false;
        }

        po::notify(vm);

        Options parsed_opts(vm);
        opts = parsed_opts;

        return true;
    } catch(po::error &e) {
        cerr << "Error parsing command line: " << e.what() << endl << endl
             << visible_options << endl;
        return false;
    }
}

} // namespace cortisol
