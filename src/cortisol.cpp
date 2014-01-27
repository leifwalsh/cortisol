/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include "cortisol.h"

#include <unistd.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "runner.h"
#include "thread_interrupter.h"

namespace cortisol {

namespace core {

void execute_runners(const std::vector<std::unique_ptr<Runner> > &runners) {
    std::vector<std::thread> threads;
    std::transform(runners.begin(), runners.end(), std::back_inserter(threads),
                   [](const std::unique_ptr<cortisol::core::Runner> &runner) {
                       return std::move(std::thread(std::mem_fn(&cortisol::core::Runner::run), runner.get()));
                   });
    std::thread output_thread([&runners]() {
            for (size_t lines_since_header = (sleep(1), std::numeric_limits<size_t>::max());
                 std::any_of(runners.begin(), runners.end(), std::mem_fn(&Runner::is_running));
                 cortisol::core::ThreadInterrupter::check_for_interrupt(), usleep(1000000)) {
                if (lines_since_header > 24) {
                    const std::unique_ptr<Runner> &runner = *runners.begin();
                    runner->header();
                    lines_since_header = 0;
                }
                std::vector<size_t> lines_printed(runners.size());
                std::transform(runners.begin(), runners.end(), lines_printed.begin(), std::mem_fn(&Runner::report));
                lines_since_header += std::accumulate(lines_printed.begin(), lines_printed.end(), 0);
            }
            std::for_each(runners.begin(), runners.end(), std::mem_fn(&Runner::total));
        });
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    output_thread.join();
}

} // namespace core

} // namespace cortisol
