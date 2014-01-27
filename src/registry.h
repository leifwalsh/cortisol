/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <algorithm>
#include <map>
#include <string>

#include <boost/program_options.hpp>

#include "exceptions/already_registered.h"
#include "options.h"

namespace cortisol {

namespace core {

namespace po = boost::program_options;

template <class RegisteredType>
class Registry : protected std::map<std::string, RegisteredType *> {
    Registry(const Registry &) = delete;
    Registry& operator=(const Registry &) = delete;

  protected:
    typedef std::pair<std::string, RegisteredType *> PairType;
    typedef std::map<std::string, RegisteredType *> MapType;

    Registry() : std::map<std::string, RegisteredType *>() {}

    virtual const std::string &name() const = 0;

  public:
    void register_me(const std::string &n, RegisteredType *reg) {
        if ((*this)[n]) {
            throw AlreadyRegistered(n);
        }
        (*this)[n] = reg;
    }

    po::options_description all_options() {
        po::options_description opts(name());
        std::for_each(this->begin(), this->end(),
                      [&opts](const PairType &p) {
                          opts.add(p.second->options_description());
                      });
        return opts;
    }
};

} // namespace core

} // namespace cortisol
