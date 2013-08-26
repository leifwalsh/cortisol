/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <atomic>
#include <iomanip>
#include <mutex>
#include <string>

#include "output.h"
#include "timing.h"

namespace cortisol {

using std::atomic;
using std::string;

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
        bool _total;
        T _iteration;
        T _cumulative;
        double _period;
        double _elapsed;
      public:
        output_line(T iteration, T cumulative, double period, double elapsed) : _total(false), _iteration(iteration), _cumulative(cumulative), _period(period), _elapsed(elapsed) {}
        output_line(T cumulative, double elapsed) : _total(true), _iteration(0), _cumulative(cumulative), _period(0), _elapsed(elapsed) {}

        class int_fmt {
          public:
            template<typename ostream_type>
            friend inline ostream_type &operator<<(ostream_type &os, const int_fmt &h) {
                os << out::pad(10);
                return os;
            }
        };
        class double_fmt {
          public:
            template<typename ostream_type>
            friend inline ostream_type &operator<<(ostream_type &os, const double_fmt &h) {
                os << out::pad(16) << std::fixed << std::setprecision(4);
                return os;
            }
        };

        template<typename ostream_type>
        friend inline ostream_type &operator<<(ostream_type &os, const output_line &line) {
            using out::ofs;

            if (line._total) {
                os << int_fmt() << "total" << ofs
                   << double_fmt() << "       " << ofs
                   << double_fmt() << "       " <<  ofs;
            } else {
                os << int_fmt() << line._iteration << ofs
                   << double_fmt() << line._period << ofs
                   << double_fmt() << line._iteration / line._period << ofs;
            }

            os << int_fmt() << line._cumulative << ofs
               << double_fmt() << line._elapsed <<  ofs
               << double_fmt() << line._cumulative / line._elapsed;

            return os;
        }

        class header {
          public:
            template<typename ostream_type>
            friend inline ostream_type &operator<<(ostream_type &os, const header &h) {
                using out::ofs;

                os << int_fmt() << "i_ops (#)" << ofs
                   << double_fmt() << "i_time (s)" << ofs
                   << double_fmt() << "i_rate (/s)" << ofs
                   << int_fmt() << "c_ops (#)" << ofs
                   << double_fmt() << "c_time (s)" << ofs
                   << double_fmt() << "c_rate (/s)";
                return os;
            }
        };
    };

    static typename output_line::header header() {
        return typename output_line::header();
    }

    output_line report(timestamp_t ti) {
        double period = ts_to_secs(ti - _last_t);
        double elapsed = ts_to_secs(ti - _t0);
        _last_t = ti;

        T this_val = atomic<T>::load();
        T delta = this_val - _last_val;
        _last_val = this_val;

        return output_line(delta, this_val, period, elapsed);
    }

    output_line total(timestamp_t ti) {
        double elapsed = ts_to_secs(ti - _t0);
        T this_val = atomic<T>::load();

        return output_line(this_val, elapsed);
    }
};

} // namespace cortisol
