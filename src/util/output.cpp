/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include <string>

#include <boost/program_options.hpp>

#include "../setup.h"

namespace cortisol {

namespace util {

namespace out {

bool pad_output;
string ofs;
string ors;
double output_period;
size_t header_period;

namespace po = boost::program_options;

class DisplayOptions : public core::Setup {
  public:
    DisplayOptions() : core::Setup("Display") {}

    void add_options(po::options_description &desc) {
        desc.add_options()
                ("pad-output",    po::value(&pad_output)->default_value(true),   "Pad output fields into columns.")
                ("ofs",           po::value(&ofs)->default_value("\t"),          "Output field separator.")
                ("ors",           po::value(&ors)->default_value("\n"),          "Output record separator.")
                ("output-period", po::value(&output_period)->default_value(1.0), "Seconds between output.")
                ("header-period", po::value(&header_period)->default_value(20),  "Lines between printing headers.")
                ;
    }
} displayOptions;

} // namespace out

} // namespace util

} // namespace cortisol
