/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <exception>

namespace cortisol {

namespace core {

class UnimplementedException : public std::exception {
  public:
    virtual const char *what() const noexcept { return "unimplemented"; }
};

} // namespace core

} // namespace cortisol
