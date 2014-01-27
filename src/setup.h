/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "exceptions/unimplemented.h"
#include "options.h"
#include "registry.h"
#include "runner.h"

namespace cortisol {

namespace core {

class Setup : protected OptionsDefiner {
  protected:
    const std::string _name;

  public:
    Setup(const std::string &name);
    virtual ~Setup() {}

    virtual po::options_description options_description() {
        po::options_description desc(_name);
        add_core_options(desc);
        add_options(desc);
        return desc;
    }
    
    virtual void setup() {}

    virtual void generate_setup_runners(std::vector<std::unique_ptr<Runner> > &runners) {}
};

class SetupRegistry : public Registry<Setup> {
  public:
    const std::string &name() const {
        static const std::string n("Setup");
        return n;
    }

    void setup() {
        std::for_each(begin(), end(),
                      [](const PairType &p) {
                          p.second->setup();
                      });
    }

    void generate_setup_runners(std::vector<std::unique_ptr<Runner> > &runners) {
        std::for_each(begin(), end(),
                      [&runners](const PairType &p) {
                          p.second->generate_setup_runners(runners);
                      });
    }

    static SetupRegistry& get();
};

} // namespace core

} // namespace cortisol
