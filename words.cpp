/* -*- mode: C++; c-file-style: "Google"; c-basic-offset: 4 -*- */

#include "words.h"

#include <stdlib.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace cortisol {

using std::ifstream;
using std::string;
using std::stringstream;
using std::vector;

class Closer {
    ifstream &_f;
  public:
    Closer(ifstream &f) : _f(f) {}
    ~Closer() { _f.close(); }
};

Wordlist::Wordlist(const string &filename) {
    ifstream file(filename.c_str());
    Closer closer(file);

    while (file.good()) {
        string line;
        getline(file, line);
        push_back(line);
    }
}

string Wordlist::randstr(int min_size) const {
    stringstream ss;
    const size_t my_size = size();
    for (int sz = 0; sz < min_size; ) {
        const string &s = at(random() % my_size);
        sz += s.size();
        ss << s;
    }
    return ss.str();
}

} // namespace cortisol
