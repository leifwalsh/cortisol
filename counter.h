/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <atomic>
#include <iomanip>
#include <mutex>
#include <string>

#include "timing.h"

namespace cortisol {

using std::atomic;
using std::string;

extern string ofs;
extern string ors;

template<typename T>
class counter : public atomic<T> {
    const timestamp_t _t0;
    T _last_val;
    timestamp_t _last_t;
  public:
    counter(timestamp_t t0) : std::atomic<T>(0), _t0(t0), _last_val(0), _last_t(t0) {}
    counter(const counter&) = delete;
    counter& operator=(const counter&) = delete;

    class output_line {
        T _iteration;
        T _cumulative;
        double _period;
        double _elapsed;
      public:
        output_line(T iteration, T cumulative, double period, double elapsed) : _iteration(iteration), _cumulative(cumulative), _period(period), _elapsed(elapsed) {}

        template<typename ostream_type>
        static void int_fmt(ostream_type &os) {
            os << std::setiosflags(std::ios_base::right)
               << std::setw(10);
        }
        template<typename ostream_type>
        static void double_fmt(ostream_type &os) {
            os << std::setiosflags(std::ios_base::right)
               << std::setw(14)
               << std::fixed
               << std::setprecision(4);
        }

        template<typename ostream_type>
        friend inline ostream_type &operator<<(ostream_type &os, const output_line &line) {
            int_fmt(os);
            os << line._iteration << ofs;
            double_fmt(os);
            os << line._period << "s" << ofs;
            double_fmt(os);
            os << line._iteration / line._period << "/s" << ofs;

            int_fmt(os);
            os << line._cumulative << ofs;
            double_fmt(os);
            os << line._elapsed << "s" << ofs;
            double_fmt(os);
            os << line._cumulative / line._elapsed << "/s";
            return os;
        }
    };

    output_line report(timestamp_t ti) {
        double period = ts_to_secs(ti - _last_t);
        double elapsed = ts_to_secs(ti - _t0);
        _last_t = ti;

        T this_val = atomic<T>::load();
        T delta = this_val - _last_val;
        _last_val = this_val;

        return output_line(delta, this_val, period, elapsed);
    }
};

} // namespace cortisol
