/// \file dve2hsc.cc
/// \brief Import a DVE (BEEM) model into the `.hsc` surface.
///
/// T2M (`dve::parse`) then M2M (`dve::to_surface`), then the `.hsc` text
/// (M2T) to stdout or `-o`. `--summary` stops after the parse and reports
/// what was seen; `--parse-only` stops silently. Exit: 0 ok, 1 parse error,
/// 3 transform error (a model the mapping does not cover yet).

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <CLI/CLI.hpp>

#include "hsc/dve/ast.hh"
#include "hsc/dve/to_surface.hh"

int main(int argc, char** argv) {
  CLI::App app{"Import a DVE (BEEM) model into the .hsc surface"};
  std::string in, out;
  bool summary = false, parse_only = false, force_order = true;
  app.add_option("input", in, "the .dve model")->required();
  app.add_option("-o,--output", out, "write the .hsc here (default: stdout)");
  app.add_option("--order", force_order,
                 "frontier layout: force (FORCE heuristic) or decl "
                 "(declaration order)")
      ->transform(CLI::CheckedTransformer(
          std::map<std::string, bool>{{"force", true}, {"decl", false}}))
      ->default_str("force");
  app.add_flag("--summary", summary, "report what the parse saw, then stop");
  app.add_flag("--parse-only", parse_only, "stop silently after the parse");
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return app.exit(e) == 0 ? 0 : 2;
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
