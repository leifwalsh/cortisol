/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <boost/program_options.hpp>

namespace cortisol {

namespace core {

namespace po = boost::program_options;

class OptionsDefiner {
  protected:
    virtual void add_core_options(po::options_description &desc) {}
    virtual void add_options(po::options_description &desc) = 0;

  public:
    virtual po::options_description options_description() = 0;
};

} // namespace core

} // namespace cortisol
