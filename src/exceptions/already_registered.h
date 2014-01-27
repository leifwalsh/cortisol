/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <exception>

namespace cortisol {

namespace core {

class AlreadyRegistered : std::exception {
    std::string _what;
  public:
    AlreadyRegistered(const std::string &name) {
        std::stringstream ss;
        ss << "already registered plugin with name " << name;
        _what = ss.str();
    }
    virtual const char *what() const noexcept {
        return _what.c_str();
    }
};

} // namespace core

} // namespace cortisol
