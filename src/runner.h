/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <exception>
#include <iostream>

#include <boost/program_options.hpp>

#include "exceptions/interrupted.h"
#include "exceptions/unimplemented.h"
#include "options.h"
#include "registry.h"
#include "thread_interrupter.h"
#include "util/counter.h"
#include "util/output.h"

namespace cortisol {

namespace core {

class Runner {
    util::counter<size_t> _steps;
    bool _running;

  public:
    Runner() : _steps(0), _running(true) {}
    virtual ~Runner() {}

    virtual const std::string &name() const = 0;
    virtual void step() { throw UnimplementedException(); }
    virtual void header() {
        std::cout << "# "
                  << util::out::pad(8) << "name"
                  << util::out::ofs << util::counter<size_t>::header()
                  << std::endl;
    }
    virtual size_t report() {
        std::cout << util::out::pad(10) << name()
                  << util::out::ofs << _steps.report(now())
                  << std::endl;
        return 1;
    }
    virtual void total() {
        std::cout << util::out::pad(10) << name()
                  << util::out::ofs << _steps.total(now())
                  << std::endl;
    }

    void run() {
        while (_running) {
            try {
                ThreadInterrupter::check_for_interrupt();
                step();
                _steps++;
            } catch (InterruptedException) {
                stop();
            } catch (UnimplementedException) {
                std::cerr << "unimplemented step()" << std::endl;
                stop();
            } catch (std::exception &e) {
                std::cerr << "caught exception " << e.what() << std::endl;
            }
        }
    }

    void stop() {
        _running = false;
    }

    bool is_running() const {
        return _running;
    }
};

namespace po = boost::program_options;

/**
 * Creates Runner objects based on a set of options it defines.
 *
 * Need to implement add_options() and generate().
 */
class RunnerFactory : protected OptionsDefiner {
  protected:
    const std::string _name;
    const std::string _cli_section;

  public:
    RunnerFactory(const std::string &name, const std::string &cli_section);

    virtual po::options_description options_description() {
        po::options_description desc(_name);
        add_core_options(desc);
        add_options(desc);
        return desc;
    }

    virtual void generate(std::vector<std::unique_ptr<Runner> > &runners) = 0;
};

/**
 * Creates N Runner objects, based on the .threads option.
 *
 * Most of the time you should just subclass this to get N identical threads.  Just implement add_options() and make().
 */
class NRunnerFactory : public RunnerFactory {
    size_t _threads;

  protected:
    virtual std::unique_ptr<Runner> make(size_t i) const = 0;

    void add_core_options(po::options_description &desc) {
        std::string s = _cli_section + ".threads";
        desc.add_options()(s.c_str(), po::value(&_threads)->default_value(_threads), "# of threads.");
    }

  public:
    NRunnerFactory(const std::string &name, const std::string &cli_section) : RunnerFactory(name, cli_section), _threads(0) {}

    void generate(std::vector<std::unique_ptr<Runner> > &runners) {
        size_t i = 0;
        std::generate_n(std::back_inserter(runners), _threads, [this, &i]() { return make(i++); });
    }
};

class RunnerFactoryRegistry : public Registry<RunnerFactory> {
  public:
    const std::string &name() const {
        static const std::string n("Runners");
        return n;
    }

    void generate(std::vector<std::unique_ptr<Runner> > &runners) {
        std::for_each(begin(), end(),
                      [&runners](const PairType &p) {
                          p.second->generate(runners);
                      });
    }

    static RunnerFactoryRegistry& get();
};

} // namespace core

} // namespace cortisol
