/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include <boost/program_options.hpp>

#include "cortisol.h"
#include "options.h"
#include "output.h"

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
    opts.seconds = 60;
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
            ("seconds",         po::value(&seconds)->default_value(seconds),                    "Time to run stressors for.")
            ;

    po::options_description all_options("General");
    all_options
            .add(conn_options)
            .add(exec_options)
            .add(Collection::options_description())
            .add(out::options_description())
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
