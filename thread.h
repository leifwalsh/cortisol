/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <atomic>

namespace cortisol {

class interrupt_exception : public std::exception {
  public:
    virtual const char *what() const noexcept { return "interrupted"; }
};

class thread_interrupter {
    std::atomic_bool _interrupted;
  public:
    thread_interrupter() : _interrupted(false) {}
    thread_interrupter(const thread_interrupter&) = delete;
    thread_interrupter &operator=(const thread_interrupter&) = delete;

    void interrupt() {
        _interrupted.store(true);
    }
    void check_for_interrupt() {
        if (_interrupted.load()) {
            throw interrupt_exception();
        }
    }
};

} // namespace cortisol
