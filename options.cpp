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

po::options_description Options::options_description() {
    po::options_description conn_options("Connection");
    conn_options.add_options()
            ("host", po::value(&host)->default_value(host), "Host to connect to.  Can also be a replica set with full \"mongodb://host1,host2,host3/?replicaSet=rsName\" syntax.")
            ;
        

    po::options_description exec_options("Execution");
    exec_options.add_options()
            ("create",          po::value(&create)->default_value(create),                      "Create and fill the collections. (skip create: --create=off)")
            ("stress",          po::value(&stress)->default_value(stress),                      "Stress an existing set of collections. (skip stress: --stress=off)")
            ("keep-database",   po::value(&keep_database)->default_value(keep_database),        "Don't drop the existing database before running.")
            ("loader",          po::value(&loader)->default_value(loader),                      "Use the bulk loader to load collections.")
            ;

    po::options_description coll_options("Collection");
    coll_options.add_options()
            ("collections",     po::value(&collections)->default_value(collections),            "# of collections.")
            ("indexes",         po::value(&indexes)->default_value(indexes),                    "# of indexes per collection.")
            ("clustering",      po::value(&clustering)->default_value(clustering),              "Whether to cluster the secondary indexes.")
            ("fields",          po::value(&fields)->default_value(fields),                      "# of fields in each document.")
            ("documents",       po::value(&documents)->default_value(documents),                "# of documents per collection.")
            ("padding",         po::value(&padding)->default_value(padding),                    "Additional bytes of padding to fill documents with.")
            ("compressibility", po::value(&compressibility)->default_value(compressibility),    "Compressibility of padding (between 0 and 1: 0 means purely random, 1 means fill with zeroes).")
            ("seconds",         po::value(&seconds)->default_value(seconds),                    "Time to run for.")
            ;

    po::options_description disp_options("Display");
    disp_options.add_options()
            ("pad-output",       po::value(&pad_output)->default_value(pad_output),             "Pad output fields into columns.")
            ("ofs",              po::value(&ofs)->default_value(ofs),                           "Output field separator.")
            ("ors",              po::value(&ors)->default_value(ors),                           "Output record separator.")
            ("output-period",    po::value(&output_period)->default_value(output_period),       "Seconds between output.")
            ("header-frequency", po::value(&header_frequency)->default_value(header_frequency), "Lines between printing headers.")
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

        return true;
    } catch(po::error &e) {
        cerr << "Error parsing command line: " << e.what() << endl << endl
             << visible_options << endl;
        return false;
    }
}

} // namespace cortisol
