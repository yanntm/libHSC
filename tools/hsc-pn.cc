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
#include <chrono>
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

// --- the budget: UNKNOWN for every open property when the alarm fires ---

std::vector<std::string> g_unknown;            // "UNKNOWN <name>\n" per property, in order
std::vector<char> g_open;                       // 1 while the property is open
volatile std::sig_atomic_t g_print_unknown = 0;

extern "C" void on_alarm(int) {
#ifndef _WIN32
  if (g_print_unknown) {
    for (std::size_t i = 0; i < g_unknown.size(); ++i) {
      if (!g_open[i]) continue;
      const std::string& s = g_unknown[i];
      (void)!write(1, s.data(), s.size());
    }
  }
#endif
  _exit(0);
}

void print_open(std::ostream& out) {
  if (!g_print_unknown) return;
  for (std::size_t i = 0; i < g_unknown.size(); ++i) {
    if (g_open[i]) out << g_unknown[i];
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
  std::unique_ptr<SparsePetriNet<int>> net;
  PNETIO<int>::Blocks blocks;  // the optional named blocks of a PNET
  try {
    net.reset(pnml.empty() ? PNETIO<int>::read(pnet, &blocks)
                           : loadXML<int>(pnml));
    if (!net) {
      std::cerr << "failed to load the net\n";
      return 1;
    }
  } catch (const std::string& e) {
    std::cerr << e << '\n';
    return 1;
  } catch (const char* e) {  // the PNML handler throws literals on a net it cannot read
    std::cerr << e << '\n';
    return 1;
  } catch (const std::exception& e) {  // e.g. a marking beyond the integer width
    std::cerr << "cannot load " << (pnml.empty() ? pnet : pnml) << ": "
              << e.what() << '\n';
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

  // TMULT: what each transition stands for in the producer's original net.
  // Stored as weight - 1, so an absent entry is 1 (INTEROP.md section 3).
  std::vector<long long> mult;
  // A net read from PNET went through a producer's transformations. Its arcs
  // are the arcs of the net the caller cares about only if the producer said
  // so, which it does by attaching TMULT (and dropping it when a step could
  // not account for what it removed). A net read from PNML is the original.
  bool arcs_countable = pnml.empty() ? false : true;
  if (const MatrixCol<int>* tm = PNETIO<int>::find(blocks, "TMULT")) {
    arcs_countable = true;
    if (tm->getRowCount() != net->getTransitionCount() ||
        tm->getColumnCount() != 1) {
      std::cerr << "TMULT is " << tm->getRowCount() << " x "
                << tm->getColumnCount() << ", expected "
                << net->getTransitionCount() << " x 1\n";
      return 1;
    }
    mult.assign(net->getTransitionCount(), 1);
    const SparseArray<int>& col = tm->getColumn(0);
    for (std::size_t k = 0; k < col.size(); ++k) {
      if (col.valueAt(k) < 0) {
        std::cerr << "TMULT holds a negative weight for transition "
                  << col.keyAt(k) << '\n';
        return 1;
      }
      mult[col.keyAt(k)] = 1LL + col.valueAt(k);
    }
    if (!quiet) {
      std::cerr << "TMULT: " << col.size() << " of "
                << net->getTransitionCount()
                << " transitions stand for more than one\n";
    }
  }

  // PDROP: what the constant places the producer removed were holding. Their
  // tokens are in every marking, so they belong in the two token values.
  std::vector<long long> dropped_tokens;
  if (const MatrixCol<int>* pd = PNETIO<int>::find(blocks, "PDROP")) {
    if (pd->getColumnCount() > 0) {
      const SparseArray<int>& col = pd->getColumn(0);
      for (std::size_t k = 0; k < col.size(); ++k) {
        dropped_tokens.push_back(col.valueAt(k));
      }
    }
    if (!quiet) {
      long long sum = 0;
      for (const long long v : dropped_tokens) sum += v;
      std::cerr << "PDROP: " << dropped_tokens.size()
                << " removed constant places holding " << sum << " tokens\n";
    }
  }

  // PCOEF: a place standing for K places of a free component the producer
  // fused. A marking of v there represents C(v+K-1, K-1) markings of the net
  // it came from, which the surface folds into its count when the model
  // declares it (`(leaf-weight NAME K)`).
  std::string weight_forms;
  if (const MatrixCol<int>* pc = PNETIO<int>::find(blocks, "PCOEF")) {
    if (pc->getRowCount() != net->getPlaceCount() || pc->getColumnCount() != 1) {
      std::cerr << "PCOEF is " << pc->getRowCount() << " x "
                << pc->getColumnCount() << ", expected " << net->getPlaceCount()
                << " x 1\n";
      return 1;
    }
    const SparseArray<int>& col = pc->getColumn(0);
    for (std::size_t k = 0; k < col.size(); ++k) {
      if (col.valueAt(k) < 0) {
        std::cerr << "PCOEF holds a negative coefficient for place "
                  << col.keyAt(k) << '\n';
        return 1;
      }
      weight_forms += "(leaf-weight " + net->getPnames()[col.keyAt(k)] + ' ' +
                      std::to_string(1 + col.valueAt(k)) + ")\n";
    }
    if (!quiet && col.size() != 0) {
      std::cerr << "PCOEF: " << col.size() << " of " << net->getPlaceCount()
                << " places stand for a fused free component\n";
    }
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
  const bool any_ctl = std::any_of(
      properties.begin(), properties.end(), [](const petri::expr::Property& p) {
        return p.kind == petri::expr::PropertyKind::CTL;
      });

  hsc::petri::emit_options opts;
  opts.exam = hsc::petri::examination::model_only;
  // A transition that cannot change the marking adds nothing to the
  // fixpoint, and its guard is read from the net for deadlock and for arc
  // counting; but it is an edge of the reachability graph — a self-loop, an
  // infinite path — which CTL cannot do without.
  opts.skip_no_effect = !any_ctl;
  opts.bound = bound;
  int effective_bound = bound;
  for (int m : net->getMarks()) effective_bound = std::max(effective_bound, m + 1);

  std::ostringstream model;
  hsc::petri::to_surface(model, *net, units, opts);
  model << weight_forms;
  if (force) model << "(reorder-force)\n";
  model << "(reach R saturate)\n";
  if (!export_hsc.empty()) {
    std::ofstream f(export_hsc, std::ios::binary);
    f << model.str();
  }
  for (const petri::expr::Property& p : properties) g_unknown.push_back("UNKNOWN " + p.name + "\n");
  g_open.assign(properties.size(), 1);
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
    if (!mult.empty()) solver.set_multiplicities(std::move(mult));
    solver.set_arcs_countable(arcs_countable);
    if (!dropped_tokens.empty()) solver.set_dropped_tokens(std::move(dropped_tokens));
    for (const std::string& l : solver.feed(model.str())) {
      if (verbose) std::cerr << l << '\n';
    }
    // Reachability questions run to their end, in order. CTL properties run
    // in rounds under a per-property deadline that grows fourfold each round
    // (a 64th of the budget first), so the cheap ones are answered before
    // an expensive one can eat the budget; a property stopped by its
    // deadline resumes from what it memoised.
    using clock = std::chrono::steady_clock;
    const clock::time_point start = clock::now();
    const std::optional<clock::time_point> end =
        total_time > 0 ? std::optional(start + std::chrono::seconds(total_time)) : std::nullopt;
    for (std::size_t i = 0; i < properties.size(); ++i) {
      if (properties[i].kind == petri::expr::PropertyKind::CTL) continue;
      if (solver.answer(properties[i], std::cout)) g_open[i] = 0;
    }
    if (any_ctl && end) {
      // Fair shares in two passes: a property may take twice the remaining
      // budget divided by the properties still open; what it leaves is shared
      // again by the second pass; the last pass gives the rest to one. Work
      // interrupted is lost except what was memoised, so passes are few.
      std::vector<std::size_t> open;
      for (std::size_t i = 0; i < properties.size(); ++i)
        if (g_open[i] && properties[i].kind == petri::expr::PropertyKind::CTL) open.push_back(i);
      for (int pass = 0; pass < 3 && !open.empty(); ++pass) {
        std::vector<std::size_t> still;
        for (std::size_t k = 0; k < open.size(); ++k) {
          const clock::time_point now = clock::now();
          if (now >= *end) { still.insert(still.end(), open.begin() + static_cast<std::ptrdiff_t>(k), open.end()); break; }
          const double remaining = std::chrono::duration<double>(*end - now).count();
          const double left = static_cast<double>(open.size() - k);
          const double slice = pass == 0 ? std::max(1.0, 2.0 * remaining / left)
                               : pass == 1 ? std::max(1.0, remaining / left)
                                           : remaining;
          const auto d = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(slice));
          solver.set_deadline(std::min(*end, now + d));
          if (solver.answer(properties[open[k]], std::cout)) g_open[open[k]] = 0;
          else still.push_back(open[k]);
        }
        solver.set_deadline(std::nullopt);
        open = still;
      }
    } else if (any_ctl) {
      for (std::size_t i = 0; i < properties.size(); ++i) {
        if (properties[i].kind == petri::expr::PropertyKind::CTL && solver.answer(properties[i], std::cout))
          g_open[i] = 0;
      }
    }
    print_open(std::cout);
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
