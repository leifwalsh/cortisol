/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <iomanip>
#include <memory>
#include <string>

namespace cortisol {

namespace util {

namespace out {

extern bool pad_output;
extern std::string ofs;
extern std::string ors;
extern double output_period;
extern size_t header_period;

class pad {
    size_t _n;
  public:
    pad(size_t n) : _n(n) {}
    template<class ostream_type>
    friend inline ostream_type &operator<<(ostream_type &os, const pad &p) {
        if (pad_output) {
            os << std::setiosflags(std::ios_base::right) << std::setw(p._n);
        }
        return os;
    }
};

} // namespace out

} // namespace util

} // namespace cortisol
