/// \file dve2hsc.cc
/// \brief Import a DVE (BEEM) model into the `.hsc` surface.
///
/// T2M (`dve::parse`) then M2M (`dve::to_surface`), then the `.hsc` text
/// (M2T) to stdout or `-o`. `--summary` stops after the parse and reports
/// what was seen; `--parse-only` stops silently. Exit: 0 ok, 1 parse error,
/// 3 transform error (a model the mapping does not cover yet).

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "hsc/dve/ast.hh"
#include "hsc/dve/to_surface.hh"

int main(int argc, char** argv) {
  std::string in, out;
  bool summary = false, parse_only = false, force_order = true;
  constexpr const char* usage =
      "usage: %s <in.dve> [-o out.hsc] [--order=force|decl] "
      "[--summary|--parse-only]\n";
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--summary") summary = true;
    else if (a == "--parse-only") parse_only = true;
    else if (a == "--order=force") force_order = true;
    else if (a == "--order=decl") force_order = false;
    else if (a == "-o" && i + 1 < argc) out = argv[++i];
    else if (in.empty()) in = a;
    else {
      std::fprintf(stderr, usage, argv[0]);
      return 2;
    }
  }
  if (in.empty()) {
    std::fprintf(stderr, usage, argv[0]);
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
      return 0;
    }
    if (parse_only) return 0;
    const hsc::dve::surface_model sm = hsc::dve::to_surface(m, force_order);
    if (out.empty()) {
      hsc::dve::print_hsc(std::cout, sm);
    } else {
      std::ofstream o(out, std::ios::binary);
      if (!o) {
        std::cerr << "cannot write " << out << '\n';
        return 2;
      }
      hsc::dve::print_hsc(o, sm);
    }
    return 0;
  } catch (const hsc::dve::parse_error& e) {
    std::cerr << in << ": parse error: " << e.what() << '\n';
    return 1;
  } catch (const hsc::dve::transform_error& e) {
    std::cerr << in << ": transform error: " << e.what() << '\n';
    return 3;
  }
}
