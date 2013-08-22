/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <sched.h>

#include <functional>
#include <iostream>
#include <memory>
#include <thread>

#include "timing.h"

namespace cortisol {

using std::cerr;
using std::endl;
using std::thread;
using std::unique_ptr;

class alarm {
    const timestamp_t _t0;
    volatile bool _done;
    unique_ptr<thread> _thread;
  public:
    static constexpr bool on = false;
    static constexpr double threshold = 0.100;
    alarm() :
            _t0(now()),
            _done(false),
            _thread(on ? new std::thread(std::mem_fn(&alarm::run), this) : NULL) {}
    ~alarm() {
        _done = true;
        if (_thread) {
            _thread->join();
        }
    }
    void run() {
        while (!_done) {
            sched_yield();
            if (ts_to_secs(now() - _t0) > threshold) {
                cerr << "alarm" << endl;
                break;
            }
        }
    }
};

} // namespace cortisol
