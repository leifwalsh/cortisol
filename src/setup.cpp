/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include "setup.h"

#include <memory>
#include <string>

namespace cortisol {

namespace core {

SetupRegistry& SetupRegistry::get() {
    static std::unique_ptr<SetupRegistry> ptr;
    if (!ptr) {
        ptr.reset(new SetupRegistry);
    }
    return *ptr;
}
    
Setup::Setup(const std::string &name) : _name(name) {
    SetupRegistry::get().register_me(name, this);
}

} // namespace core

} // namespace cortisol
