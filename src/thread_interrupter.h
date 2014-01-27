/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <atomic>

#include "exceptions/interrupted.h"

namespace cortisol {

namespace core {

class ThreadInterrupter {
    std::atomic_bool _interrupted;

    ThreadInterrupter() : _interrupted(false) {}

    static ThreadInterrupter& get();

  public:
    ThreadInterrupter(const ThreadInterrupter&) = delete;
    ThreadInterrupter &operator=(const ThreadInterrupter&) = delete;

    static void interrupt() {
        get()._interrupted.store(true);
    }

    static void check_for_interrupt() {
        if (get()._interrupted.load()) {
            throw InterruptedException();
        }
    }
};

} // namespace core

} // namespace cortisol
