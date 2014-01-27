/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include <errno.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include "cortisol.h"
#include "runner.h"
#include "setup.h"

namespace po = boost::program_options;

bool cortisol::verbose = false;

int main(int argc, const char *argv[]) {
    cortisol::core::SetupRegistry &global_setup_registry = cortisol::core::SetupRegistry::get();
    cortisol::core::RunnerFactoryRegistry &global_runner_factory_registry = cortisol::core::RunnerFactoryRegistry::get();

    po::options_description visible_options("Options");
    visible_options.add_options()
            ("help,h", "Get help.")
            ("verbose,v", po::value(&cortisol::verbose)->default_value(false), "More verbose output.")
            ;

    visible_options
            .add(global_setup_registry.all_options())
            .add(global_runner_factory_registry.all_options())
            ;

    po::options_description all_options;
    all_options.add_options()
            ("response-file", po::value<std::vector<std::string> >(), "Config file.")
            ;
    all_options.add(visible_options);

    try {
        po::variables_map cli_vm;
        auto cli_parsed = po::parse_command_line(argc, argv, all_options, 0,
                                                 [](const std::string &s) -> std::pair<std::string, std::string> {
                                                     if (s[0] == '@') {
                                                         return std::make_pair(std::string("response-file"), s.substr(1));
                                                     } else {
                                                         return std::pair<std::string, std::string>();
                                                     }
                                                 });
        po::store(cli_parsed, cli_vm);

        po::variables_map vm;
        if (cli_vm.count("response-file")) {
            const std::vector<std::string> files = cli_vm["response-file"].as<std::vector<std::string> >();
            std::for_each(files.begin(), files.end(),
                          [all_options, &vm](const std::string &filename) {
                              std::ifstream ifs(filename.c_str());
                              if (ifs.good()) {
                                  po::store(po::parse_config_file(ifs, all_options), vm);
                              }
                          });
        }
        po::store(cli_parsed, vm);

        if (vm.count("help")) {
            std::cout << argv[0] << ":" << std::endl
                      << visible_options << std::endl;
            return false;
        }

        po::notify(vm);
    } catch(po::error &e) {
        std::cerr << "Error parsing command line: " << e.what() << std::endl << std::endl
             << visible_options << std::endl;
        return EINVAL;
    }

    global_setup_registry.setup();

    {
        std::vector<std::unique_ptr<cortisol::core::Runner> > setup_runners;
        global_setup_registry.generate_setup_runners(setup_runners);

        cortisol::core::execute_runners(setup_runners);
    }

    {
        std::vector<std::unique_ptr<cortisol::core::Runner> > runners;
        global_runner_factory_registry.generate(runners);

        cortisol::core::execute_runners(runners);
    }

    return 0;
}
