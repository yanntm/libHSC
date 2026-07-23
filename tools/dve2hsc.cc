/// \file dve2hsc.cc
/// \brief Import a DVE (BEEM) model into the `.hsc` surface.
///
/// T2M: `dve::parse` reads the model. For now the tool stops there and
/// reports what it saw (`--summary`); the M2M transform to surface forms and
/// the `.hsc` serialization land next, on the same interface.

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "hsc/dve/ast.hh"

int main(int argc, char** argv) {
  std::string in;
  bool summary = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--summary") summary = true;
    else if (in.empty()) in = a;
    else {
      std::fprintf(stderr, "usage: %s <in.dve> [--summary]\n", argv[0]);
      return 2;
    }
  }
  if (in.empty()) {
    std::fprintf(stderr, "usage: %s <in.dve> [--summary]\n", argv[0]);
    return 2;
  }

  std::ifstream f(in, std::ios::binary);
  if (!f) {
    std::cerr << "cannot open " << in << '\n';
    return 2;
  }
  std::ostringstream buf;
  buf << f.rdbuf();

  try {
    const hsc::dve::model m = hsc::dve::parse(buf.str());
    if (summary) {
      std::size_t states = 0, trans = 0, locals = 0;
      for (const auto& p : m.processes) {
        states += p.states.size();
        trans += p.transitions.size();
        locals += p.locals.size();
      }
      std::cout << in << ": " << m.processes.size() << " processes, " << states
                << " states, " << trans << " transitions, "
                << m.globals.size() << " globals, " << locals << " locals, "
                << m.channels.size() << " channels, "
                << (m.async ? "async" : "sync") << '\n';
    }
    return 0;
  } catch (const hsc::dve::parse_error& e) {
    std::cerr << in << ": parse error: " << e.what() << '\n';
    return 1;
  }
}
