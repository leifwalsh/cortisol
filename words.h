/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#pragma once

#include <string>
#include <vector>

namespace cortisol {

using std::string;
using std::vector;

class Wordlist : public vector<string> {
  public:
    Wordlist(const string &filename);
    string randstr(int min_size) const;
};

} // namespace cortisol
