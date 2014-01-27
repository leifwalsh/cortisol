/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include "runner.h"

#include <memory>
#include <string>

namespace cortisol {

namespace core {

RunnerFactoryRegistry& RunnerFactoryRegistry::get() {
    static std::unique_ptr<RunnerFactoryRegistry> ptr;
    if (!ptr) {
        ptr.reset(new RunnerFactoryRegistry);
    }
    return *ptr;
}

RunnerFactory::RunnerFactory(const std::string &name, const std::string &cli_section)
        : _name(name),
          _cli_section(cli_section) {
    RunnerFactoryRegistry::get().register_me(name, this);
}

} // namespace core

} // namespace cortisol
