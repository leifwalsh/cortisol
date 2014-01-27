/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <memory>
#include <vector>

#include "runner.h"

namespace cortisol {

extern bool verbose;

namespace core {

void execute_runners(const std::vector<std::unique_ptr<Runner> > &runners);

} // namespace core

} // namespace cortisol
