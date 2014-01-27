/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include <memory>
#include <mutex>

#include "thread_interrupter.h"

namespace cortisol {

namespace core {

ThreadInterrupter& ThreadInterrupter::get() {
    static std::unique_ptr<ThreadInterrupter> ptr;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    if (!ptr) {
        ptr.reset(new ThreadInterrupter);
    }
    return *ptr;
}

} // namespace core

} // namespace cortisol
