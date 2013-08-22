/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include <signal.h>
#include <sysexits.h>
#include <unistd.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "mongo/client/dbclient.h"

#include "collection.h"
#include "cortisol.h"
#include "options.h"
#include "thread.h"
#include "timing.h"

using std::cerr;
using std::endl;

namespace cortisol {

using std::cout;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

thread_interrupter interrupter;
void int_handler(int sig) {
    interrupter.interrupt();
    signal(sig, SIG_DFL);
}

namespace out {

bool pad_output;
string ofs;
string ors;

};

static string collname(size_t i) {
    stringstream ss;
    ss << "stress_test" << i << ".coll";
    return ss.str();
}

void run(const Options &opts) {
    out::pad_output = opts.pad_output;
    out::ofs = opts.ofs;
    out::ors = opts.ors;

    interrupter.check_for_interrupt();
    {
        vector<Collection> colls;
        size_t i = 0;
        std::generate_n(std::back_inserter(colls), opts.collections,
                        [opts, &i]() {
                            return std::move(Collection(opts, collname(i++)));
                        });
        if (opts.create) {
            if (!opts.keep_database) {
                std::for_each(colls.begin(), colls.end(), std::mem_fn(&Collection::drop));
            }
            interrupter.check_for_interrupt();
            vector<std::thread> threads;
            std::transform(colls.begin(), colls.end(), std::back_inserter(threads),
                           [opts](Collection &coll) {
                               return std::move(std::thread(std::mem_fn(&Collection::fill), &coll));
                           });
            std::for_each(threads.begin(), threads.end(), std::mem_fun_ref(&std::thread::join));
            interrupter.check_for_interrupt();
        }
    }
    interrupter.check_for_interrupt();
    if (opts.stress) {
        timestamp_t t0 = now();
        {
            vector<unique_ptr<CollectionRunner> > runners;
            for (size_t i = 0; i < opts.collections; ++i) {
                string coll = collname(i);
                size_t id = 0;
                std::generate_n(std::back_inserter(runners), UpdateRunner::threads,
                                [opts, coll, &id, t0]() -> unique_ptr<CollectionRunner> {
                                    return unique_ptr<CollectionRunner>(new UpdateRunner(opts, coll, id++, t0));
                                });
                id = 0;
                std::generate_n(std::back_inserter(runners), PointQueryRunner::threads,
                                [opts, coll, &id, t0]() -> unique_ptr<CollectionRunner> {
                                    return unique_ptr<CollectionRunner>(new PointQueryRunner(opts, coll, id++, t0));
                                });
                id = 0;
                std::generate_n(std::back_inserter(runners), RangeQueryRunner::threads,
                                [opts, coll, &id, t0]() -> unique_ptr<CollectionRunner> {
                                    return unique_ptr<CollectionRunner>(new RangeQueryRunner(opts, coll, id++, t0));
                                });
            }
            vector<std::thread> threads;
            std::transform(runners.begin(), runners.end(), std::back_inserter(threads),
                           [](const unique_ptr<CollectionRunner> &runner) {
                               return std::move(std::thread(std::ref(*runner)));
                           });

            try {
                double elapsed = 0.0;
                for (int i = 0; ; interrupter.check_for_interrupt(), ++i) {
                    usleep(std::min((opts.seconds - elapsed), opts.output_period) * 1000000);
                    timestamp_t ti = now();
                    elapsed = ts_to_secs(ti - t0);
                    if (elapsed >= opts.seconds) {
                        break;
                    }
                    if (i % opts.header_frequency == 0 ||
                        (i == 0 && opts.header_frequency >= 0)) {
                        CollectionRunner::header(cout);
                    }
                    std::for_each(runners.begin(), runners.end(),
                                  [ti](const unique_ptr<CollectionRunner> &runner) {
                                      runner->report(cout, ti);
                                  });
                }
            } catch (interrupt_exception) {
                std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
                throw;
            }
                
            std::for_each(runners.begin(), runners.end(), [](const unique_ptr<CollectionRunner> &runner) { runner->stop(); });
            std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));

            timestamp_t t1 = now();
            cout << endl << "# TOTALS:" << endl;
            std::for_each(runners.begin(), runners.end(),
                          [t1](const unique_ptr<CollectionRunner> &runner) {
                              runner->total(cout, t1);
                          });
        }
    }
}

} // namespace cortisol


int main(int argc, const char *argv[]) {
    cortisol::Options opts = cortisol::Options::default_options();
    bool ok = cortisol::parse_cmdline(argc, argv, opts);
    if (!ok) {
        return EX_USAGE;
    }

    signal(SIGINT, cortisol::int_handler);
    try {
        cortisol::run(opts);
    } catch (cortisol::interrupt_exception) {
        // ok
    } catch (const mongo::DBException &e) {
        cerr << "caught " << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
