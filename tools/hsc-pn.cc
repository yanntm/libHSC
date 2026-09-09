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
#include <sys/resource.h>
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
#include "hsc/petri/invariants.hh"
#include "hsc/order/bandwidth.hh"
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
  std::string pnml, pnet, props, syntax = "auto", shape = "nupn", export_hsc, deadlock, shape_file, export_shape;
  bool force = false, reverse = false, states = false, max_tokens = false, print_unknown = false, quiet = false,
       verbose = false, witness = false, shape_only = false, cover = false;
  int invariants_time = 0;
  long long seed = 1;
  int bound = 2, total_time = 0;
  auto* in_opt = app.add_option("-i,--pnml", pnml, "PNML P/T net (with its NUPN unit tree when present)")
                     ->check(CLI::ExistingFile);
  app.add_option("--net", pnet, "PNET binary net (places p<i>, transitions t<i>)")
      ->check(CLI::ExistingFile)->excludes(in_opt);
  app.add_option("--props", props, "property file: MCC XML (.xml) or s-expressions")
      ->check(CLI::ExistingFile);
  app.add_option("--propsSyntax", syntax, "auto|mcc|sexpr (default: by extension)");
  app.add_option("--shape", shape, "nupn|flat|louvain|rcm|sloan|random: the hierarchy or order (nupn falls back to flat)");
  app.add_option("--seed", seed, "the seed of --shape random (default 1)");
  app.add_flag("--force", force, "FORCE reordering after the shape");
  app.add_flag("--reverse", reverse, "mirror the shape at every level (after FORCE when both)");
  app.add_flag("--shape-only", shape_only, "build and rewrite the shape, print its signature (hsc-pn: shape sig=...), no fixpoint");
  app.add_option("--shape-file", shape_file, "take the shape from this file: a (spine …)/(balanced …) expression over the place names, as --export-shape writes it (overrides --shape)");
  app.add_option("--export-shape", export_shape, "write the shape after the rewrites (FORCE, reverse) to this file, one expression");
  app.add_option("--invariants", invariants_time, "compute the P-flows within S seconds and let them guide the louvain shape");
  app.add_option("--bound", bound, "leaf domain [0, N), raised to the max initial marking + 1");
  app.add_flag("--states", states, "the four StateSpace values");
  app.add_flag("--max-tokens", max_tokens, "the MAX_TOKEN_IN_PLACE value alone (the OneSafe examination)");
  app.add_option("--deadlock", deadlock, "a deadlock query with this name, without a property file");
  app.add_option("--totalTime", total_time, "seconds; then UNKNOWN for what is open and exit 0");
  app.add_flag("--printUnknown", print_unknown, "print UNKNOWN <name> for every unanswered property");
  app.add_flag("--witness", witness, "after each CTL verdict, its witness tree on stderr");
  app.add_flag("--cover", cover, "on a partial reachable set, look for a pumping pair ((pump R), an unboundedness witness) and answer the StateSpace values +inf when found");
  app.add_option("--export-hsc", export_hsc, "write the emitted .hsc model to this file");
  app.add_flag("-q,--quiet", quiet, "no import log on stderr");
  app.add_flag("-v,--verbose", verbose, "forward the session's own report lines to stderr");
  CLI11_PARSE(app, argc, argv);
#ifndef _WIN32
  // The recursion of the calculus is as deep as the shape (a flat spine of
  // ten thousand places is ten thousand levels): give the main thread the
  // stack that allows, up to the hard limit.
  {
    struct rlimit rl{};
    if (getrlimit(RLIMIT_STACK, &rl) == 0) {
      const rlim_t want = static_cast<rlim_t>(1) << 30;  // 1 GB
      rl.rlim_cur = rl.rlim_max == RLIM_INFINITY ? want : std::min(rl.rlim_max, want);
      setrlimit(RLIMIT_STACK, &rl);
    }
  }
  // The budget is wall time from the start: the parse, the shape and the
  // emission count too, and a net that takes minutes to load answers UNKNOWN.
  if (total_time > 0) {
    std::signal(SIGALRM, on_alarm);
    alarm(static_cast<unsigned>(total_time));
  }
#endif
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
    std::vector<hsc::petri::pflow> flows;
    if (invariants_time > 0) {
      const auto t0 = std::chrono::steady_clock::now();
      flows = hsc::petri::pflows(*net, invariants_time);
      if (verbose) {
        std::size_t widest = 0;
        long long largest = 0;
        for (const hsc::petri::pflow& f : flows) {
          widest = std::max(widest, f.terms.size());
          largest = std::max(largest, f.constant);
        }
        std::cerr << "invariants: " << flows.size() << " flows, widest support " << widest
                  << ", largest constant " << largest << ", "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - t0).count()
                  << " ms\n";
      }
    }
    units = hsc::petri::decompose(*net, flows);
  } else if (shape == "rcm" || shape == "sloan" || shape == "random") {
    const int n = static_cast<int>(net->getPlaceCount());
    const std::vector<hsc::order::louvain::edge> edges = hsc::petri::dependency_edges(*net);
    const std::vector<std::uint32_t> listing =
        shape == "rcm"   ? hsc::order::rcm(n, edges)
        : shape == "sloan" ? hsc::order::sloan(n, edges)
                           : hsc::order::random_order(n, static_cast<std::uint64_t>(seed));
    units = hsc::petri::ordered(*net, listing);
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
  if (!shape_file.empty()) {
    // The shape verbatim: the file holds one expression, with or without the
    // `(shape` wrapper of a spec; a trailing (reorder-…) still applies.
    std::ifstream f(shape_file);
    std::stringstream buf;
    buf << f.rdbuf();
    std::string form = buf.str();
    const std::size_t a = form.find('(');
    if (a == std::string::npos) { std::cerr << "no shape expression in " << shape_file << '\n'; return 2; }
    form = form.substr(a);
    if (form.rfind("(shape", 0) == 0) {
      form = form.substr(form.find('(', 1));
      const std::size_t z = form.rfind(')');
      if (z != std::string::npos) form = form.substr(0, z);  // the wrapper's own paren
    }
    while (!form.empty() && (form.back() == '\n' || form.back() == ' ' || form.back() == '\r')) form.pop_back();
    opts.shape_form = form;
  }
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
  if (reverse) model << "(reorder-reverse)\n";
  if (!shape_only) model << "(reach R saturate)\n";
  // the rewritten spec comes back through the session, for the shape signature
  if (shape_only || verbose) model << "(print-spec)\n";
  if (!export_hsc.empty()) {
    std::ofstream f(export_hsc, std::ios::binary);
    f << model.str();
  }
  for (const petri::expr::Property& p : properties) g_unknown.push_back("UNKNOWN " + p.name + "\n");
  g_open.assign(properties.size(), 1);
  g_print_unknown = print_unknown;

  // --- the session: model and fixpoint, then one question at a time ---
  try {
    hsc::pn::solver solver(*net, effective_bound, verbose);
    if (!mult.empty()) solver.set_multiplicities(std::move(mult));
    solver.set_arcs_countable(arcs_countable);
    solver.set_witness(witness);
    if (!dropped_tokens.empty()) solver.set_dropped_tokens(std::move(dropped_tokens));
    const auto t_model = std::chrono::steady_clock::now();
    // The signature of the shape after the rewrites: FNV-1a over the
    // `(shape …)` form `print-spec` echoes back (one line). Two heuristics
    // that produce the same order and hierarchy share it — the sweep runs
    // one of them.
    std::uint64_t sig = 1469598103934665603ull;
    // The reachable set runs under the budget's deadline (four fifths of it:
    // the alarm stays the backstop): a closure that runs out returns what it
    // has, marked partial — reported, never answered from.
    if (total_time > 0 && !shape_only) {
      solver.set_deadline(t_model + std::chrono::milliseconds(total_time * 800));
    }
    bool r_partial = false, r_diverged = false, pumped = false;
    unsigned epochs = 1;
    for (const std::string& l : solver.feed(model.str())) {
      if (l == "R partial") r_partial = true;
      if (l.rfind("R diverged", 0) == 0) r_diverged = true;
      if (l.rfind("(shape", 0) == 0) {
        for (const unsigned char c : l) { sig ^= c; sig *= 1099511628211ull; }
        if (!export_shape.empty()) { std::ofstream f(export_shape); f << l << '\n'; }
      } else if (verbose && (l.empty() || l.front() != '(')) {
        std::cerr << l << '\n';
      }
    }
    // The epochs of the divergence watch: a place ran past the limit — the
    // closure broke out early — so look for a pumping pair (a proof of
    // unboundedness) once, then resume the closure under the same deadline
    // until it converges, runs out, or breaks out at the next doubling.
    while (r_partial && r_diverged && !shape_only) {
      r_diverged = false;
      if (cover) {
        for (const std::string& l : solver.feed("(pump R)")) {
          if (verbose) std::cerr << "hsc-pn: " << l << '\n';
          if (l.rfind("R pump ", 0) == 0 && l.find(" none") == std::string::npos) pumped = true;
        }
        if (pumped) break;
      }
      r_partial = false;
      ++epochs;
      for (const std::string& l : solver.feed("(reach R saturate from R)")) {
        if (l == "R partial") r_partial = true;
        if (l.rfind("R diverged", 0) == 0) r_diverged = true;
        if (verbose) std::cerr << l << '\n';
      }
    }
    solver.set_deadline(std::nullopt);
    const auto shape_signature = [&]() -> std::string {
      std::ostringstream o;
      o << std::hex << sig;
      return o.str();
    };
    if (shape_only) {
      std::cout << "hsc-pn: shape sig=" << shape_signature() << '\n';
      return 0;
    }
    if (verbose) {
      // The statistics line of the sweeps (experiments/order/SWEEP.md §3):
      // the reachable set's cost and size, its widest level, the shape.
      const double reach_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t_model).count();
      solver.set_deadline(std::nullopt);
      std::size_t nodes = 0, arcs = 0, belly = 0, belly_level = 0, belly_span = 0, level = 0;
      double reach_states = 0;
      for (const std::string& l : solver.feed("(count R)"))
        if (l.rfind("R count ", 0) == 0) reach_states = std::atof(l.c_str() + 8);
      for (const std::string& l : solver.feed("(nodes R)"))
        if (l.rfind("R nodes ", 0) == 0) nodes = std::stoull(l.substr(8));
      for (const std::string& l : solver.feed("(profile R)")) {
        const std::size_t kn = l.find(" nodes "), ka = l.find(" arcs ");
        if (kn == std::string::npos || ka == std::string::npos || l.rfind("  ", 0) != 0) continue;
        const std::size_t n = std::stoull(l.substr(kn + 7)), a = std::stoull(l.substr(ka + 6));
        std::size_t span = 1;
        const std::size_t kp = l.rfind(" (", kn);
        if (kp != std::string::npos && kp < kn) span = std::stoull(l.substr(kp + 2));
        arcs += a;
        if (n > belly) { belly = n; belly_level = level; belly_span = span; }
        ++level;
      }
      std::size_t depth = 1, nunits = std::max<std::size_t>(1, units.units.size()), widest = net->getPlaceCount();
      if (!units.units.empty()) {
        widest = 0;
        const auto walk = [&](auto&& self, const std::string& id, std::size_t d) -> void {
          const auto it = units.units.find(id);
          if (it == units.units.end()) return;
          depth = std::max(depth, d);
          widest = std::max(widest, it->second.places.size() + it->second.subunits.size());
          for (const std::string& k : it->second.subunits) self(self, k, d + 1);
        };
        walk(walk, units.root, 1);
      }
      std::cerr << "hsc-pn: stats reach_s=" << reach_s << " partial=" << (r_partial ? 1 : 0) << " epochs=" << epochs
                << " reach_states=" << reach_states
                << " reach_nodes=" << nodes << " reach_arcs=" << arcs
                << " belly_nodes=" << belly << " belly_level=" << belly_level << " belly_span=" << belly_span
                << " shape_depth=" << depth << " shape_units=" << nunits << " shape_widest=" << widest
                << " shape_sig=" << shape_signature() << '\n';
    }
    const auto note_answer = [&](std::size_t i) {
      if (verbose)
        std::cerr << "hsc-pn: answered " << properties[i].name << " at "
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - t_model).count() << '\n';
    };
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
      if (solver.answer(properties[i], std::cout)) { g_open[i] = 0; note_answer(i); }
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
          if (solver.answer(properties[open[k]], std::cout)) { g_open[open[k]] = 0; note_answer(open[k]); }
          else still.push_back(open[k]);
        }
        solver.set_deadline(std::nullopt);
        open = still;
      }
    } else if (any_ctl) {
      for (std::size_t i = 0; i < properties.size(); ++i) {
        if (properties[i].kind == petri::expr::PropertyKind::CTL && solver.answer(properties[i], std::cout)) {
          g_open[i] = 0;
          note_answer(i);
        }
      }
    }
    print_open(std::cout);
    if (r_partial) {
      // nothing is answered from a partial set: the StateSpace values would be
      // wrong, and the properties' selections unsound — unless the set proves
      // a place unbounded (a pumping pair), which answers StateSpace for good
      if (cover && states) {
        if (!pumped) {  // the deadline cut the set without a divergence note: one last look
          for (const std::string& l : solver.feed("(pump R)")) {
            if (verbose) std::cerr << "hsc-pn: " << l << '\n';
            if (l.rfind("R pump ", 0) == 0 && l.find(" none") == std::string::npos) pumped = true;
          }
        }
        if (pumped) {
          for (const char* v : {"STATES", "TRANSITIONS", "MAX_TOKEN_IN_PLACE", "MAX_TOKEN_PER_MARKING"})
            std::cout << "STATE_SPACE " << v << " +inf TECHNIQUES DECISION_DIAGRAMS SATURATION COVERABILITY" << std::endl;
          return 0;
        }
      }
      if (verbose) std::cerr << "hsc-pn: the reachable set is partial, no answers\n";
      print_open(std::cout);
      return 0;
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
