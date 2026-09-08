/// \file hsc-pn.cc
/// \brief Answer Petri net properties symbolically: PNML or PNET in, MCC XML
/// or s-expression properties in, the FORMULA line protocol out
/// (tools/README.md, "hsc-pn: design").
///
/// The tool is a driver: it loads the net, chooses the shape, emits the
/// model in the surface language, and asks the questions through
/// `pn_solver.hh`; the calculus is reached only through the surface.

#include <CLI/CLI.hpp>

#include <algorithm>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "hsc/petri/core/Log.h"
#include "hsc/petri/decompose.hh"
#include "hsc/petri/expr/Property.h"
#include "hsc/petri/io/PNETIO.h"
#include "hsc/petri/nupn.hh"
#include "hsc/petri/parse/PTNetLoader.h"
#include "hsc/petri/parse/PropertyFile.h"
#include "hsc/petri/to_surface.hh"
#include "hsc/util/errors.hh"
#include "pn_solver.hh"

namespace {

/// \brief Narrow a net loaded in 64-bit to the 32-bit one the surface uses,
/// refusing rather than truncating.
///
/// The importers and the leaf theory are `int`, but a PNML marking or arc
/// weight may exceed 32 bits (GPPP-PT-C0010N1000000000 carries markings past
/// four billion). Loading wide and checking here is what turns a silently
/// wrong answer into a refusal.
std::unique_ptr<SparsePetriNet<int>> narrow(const SparsePetriNet<long long>& wide,
                                           std::string& why) {
  constexpr long long LIMIT = 2147483647;  // INT32_MAX
  const auto check = [&](long long v, const char* what, std::size_t i) {
    if (v < 0 || v > LIMIT) {
      why = std::string(what) + ' ' + std::to_string(i) + " is " +
            std::to_string(v) + ", outside the 32-bit range of the leaf theory";
      return false;
    }
    return true;
  };
  const std::vector<long long>& wmarks = wide.getMarks();
  std::vector<int> marks(wmarks.size(), 0);
  for (std::size_t p = 0; p < wmarks.size(); ++p) {
    if (!check(wmarks[p], "the initial marking of place", p)) return nullptr;
    marks[p] = static_cast<int>(wmarks[p]);
  }
  const auto convert = [&](const MatrixCol<long long>& src, const char* what,
                           MatrixCol<int>& dst) {
    dst = MatrixCol<int>(src.getRowCount(), 0);
    for (std::size_t t = 0; t < src.getColumnCount(); ++t) {
      const SparseArray<long long>& col = src.getColumn(t);
      SparseArray<int> out;
      for (std::size_t k = 0; k < col.size(); ++k) {
        if (!check(col.valueAt(k), what, t)) return false;
        out.append(col.keyAt(k), static_cast<int>(col.valueAt(k)));
      }
      dst.appendColumn(std::move(out));
    }
    return true;
  };
  MatrixCol<int> pt, tp;
  if (!convert(wide.getFlowPT(), "a pre-arc weight of transition", pt)) return nullptr;
  if (!convert(wide.getFlowTP(), "a post-arc weight of transition", tp)) return nullptr;
  auto net = std::make_unique<SparsePetriNet<int>>(std::move(pt), std::move(tp),
                                                   std::move(marks));
  net->setName(wide.getName());
  return net;
}

// --- the budget: UNKNOWN for every open property when the alarm fires ---

std::vector<std::string> g_unknown;            // "UNKNOWN <name>\n" per property, in order
volatile std::sig_atomic_t g_next_open = 0;    // index of the first open property
volatile std::sig_atomic_t g_print_unknown = 0;

extern "C" void on_alarm(int) {
#ifndef _WIN32
  if (g_print_unknown) {
    for (std::size_t i = static_cast<std::size_t>(g_next_open); i < g_unknown.size(); ++i) {
      const std::string& s = g_unknown[i];
      (void)!write(1, s.data(), s.size());
    }
  }
#endif
  _exit(0);
}

void print_open(std::ostream& out) {
  if (!g_print_unknown) return;
  for (std::size_t i = static_cast<std::size_t>(g_next_open); i < g_unknown.size(); ++i) {
    out << g_unknown[i];
  }
  out.flush();
}

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"hsc-pn — answer Petri net properties with the HSC engine.\n"
               "Net: -i model.pnml or --net model.pnet. Properties: MCC XML or\n"
               "s-expression forms (INTEROP.md). Answers: FORMULA lines on stdout."};
  std::string pnml, pnet, props, syntax = "auto", shape = "nupn", export_hsc, deadlock;
  bool force = false, states = false, max_tokens = false, print_unknown = false, quiet = false, verbose = false;
  int bound = 2, total_time = 0;
  auto* in_opt = app.add_option("-i,--pnml", pnml, "PNML P/T net (with its NUPN unit tree when present)")
                     ->check(CLI::ExistingFile);
  app.add_option("--net", pnet, "PNET binary net (places p<i>, transitions t<i>)")
      ->check(CLI::ExistingFile)->excludes(in_opt);
  app.add_option("--props", props, "property file: MCC XML (.xml) or s-expressions")
      ->check(CLI::ExistingFile);
  app.add_option("--propsSyntax", syntax, "auto|mcc|sexpr (default: by extension)");
  app.add_option("--shape", shape, "nupn|flat|louvain: the hierarchy (nupn falls back to flat)");
  app.add_flag("--force", force, "FORCE reordering after the shape");
  app.add_option("--bound", bound, "leaf domain [0, N), raised to the max initial marking + 1");
  app.add_flag("--states", states, "the four StateSpace values");
  app.add_flag("--max-tokens", max_tokens, "the MAX_TOKEN_IN_PLACE value alone (the OneSafe examination)");
  app.add_option("--deadlock", deadlock, "a deadlock query with this name, without a property file");
  app.add_option("--totalTime", total_time, "seconds; then UNKNOWN for what is open and exit 0");
  app.add_flag("--printUnknown", print_unknown, "print UNKNOWN <name> for every unanswered property");
  app.add_option("--export-hsc", export_hsc, "write the emitted .hsc model to this file");
  app.add_flag("-q,--quiet", quiet, "no import log on stderr");
  app.add_flag("-v,--verbose", verbose, "forward the session's own report lines to stderr");
  CLI11_PARSE(app, argc, argv);
  if (pnml.empty() && pnet.empty()) {
    std::cerr << "one of -i or --net is required\n";
    return 2;
  }

  std::ostream null_log(nullptr);
  petri::setLogStream(quiet ? null_log : std::cerr);

  // --- the net and its shape ---
  // loaded in 64-bit, then narrowed with a check: a net beyond 32 bits is
  // refused, never truncated into a wrong answer
  std::unique_ptr<SparsePetriNet<int>> net;
  try {
    std::unique_ptr<SparsePetriNet<long long>> wide(
        pnml.empty() ? PNETIO<long long>::read(pnet) : loadXML<long long>(pnml));
    if (!wide) {
      std::cerr << "failed to load the net\n";
      return 1;
    }
    std::string why;
    net = narrow(*wide, why);
    if (!net) {
      std::cerr << "refusing " << (pnml.empty() ? pnet : pnml) << ": " << why
                << '\n';
      return 1;
    }
  } catch (const std::string& e) {
    std::cerr << e << '\n';
    return 1;
  } catch (const char* e) {  // the PNML handler throws literals on a net it cannot read
    std::cerr << e << '\n';
    return 1;
  }
  hsc::petri::unit_tree units;
  if (shape == "nupn") {
    if (!pnml.empty()) units = hsc::petri::read_units(pnml);
  } else if (shape == "louvain") {
    units = hsc::petri::decompose(*net);
  } else if (shape != "flat") {
    std::cerr << "unknown shape '" << shape << "'\n";
    return 2;
  }

  hsc::petri::emit_options opts;
  opts.exam = hsc::petri::examination::model_only;
  opts.bound = bound;
  int effective_bound = bound;
  for (int m : net->getMarks()) effective_bound = std::max(effective_bound, m + 1);

  std::ostringstream model;
  hsc::petri::to_surface(model, *net, units, opts);
  if (force) model << "(reorder-force)\n";
  model << "(reach R saturate)\n";
  if (!export_hsc.empty()) {
    std::ofstream f(export_hsc, std::ios::binary);
    f << model.str();
  }

  // --- the properties ---
  std::vector<petri::expr::Property> properties;
  if (!props.empty()) {
    try {
      properties = petri::loadPropertyFile<int>(props, *net, petri::propertySyntaxOf(syntax));
    } catch (const std::string& e) {
      std::cerr << e << '\n';
      return 1;
    }
  }
  if (!deadlock.empty()) {
    petri::expr::Property p;
    p.name = deadlock;
    p.kind = petri::expr::PropertyKind::Deadlock;
    properties.push_back(std::move(p));
  }
  for (const petri::expr::Property& p : properties) g_unknown.push_back("UNKNOWN " + p.name + "\n");
  g_print_unknown = print_unknown;
#ifndef _WIN32
  if (total_time > 0) {
    std::signal(SIGALRM, on_alarm);
    alarm(static_cast<unsigned>(total_time));
  }
#endif

  // --- the session: model and fixpoint, then one question at a time ---
  try {
    hsc::pn::solver solver(*net, effective_bound, verbose);
    for (const std::string& l : solver.feed(model.str())) {
      if (verbose) std::cerr << l << '\n';
    }
    for (const petri::expr::Property& p : properties) {
      if (!solver.answer(p, std::cout) && print_unknown) {
        std::cout << "UNKNOWN " << p.name << std::endl;
      }
      g_next_open = g_next_open + 1;
    }
    if (states) solver.state_space(std::cout);
    else if (max_tokens) solver.max_tokens(std::cout);
  } catch (const hsc::overflow_error& e) {
    std::cerr << "overflow: " << e.what() << " (raise --bound)\n";
    print_open(std::cout);
    if (states) std::cout << "STATE_SPACE STATES CANNOT_COMPUTE" << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  } catch (const std::string& e) {  // the vendored readers throw strings
    std::cerr << "error: " << e << '\n';
    return 1;
  } catch (const char* e) {
    std::cerr << "error: " << e << '\n';
    return 1;
  }
  return 0;
}
