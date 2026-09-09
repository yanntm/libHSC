/// \file pn_solver.hh
/// \brief Answer Petri net properties through an incremental surface session:
/// one `select` + `count` per question over the reachable set `R` (one
/// `ctl` form per CTL property), the answer decoded from the session's
/// output and printed as protocol lines
/// (tools/README.md, "hsc-pn: design").
#pragma once

#include <gmpxx.h>
#include <chrono>

#include <cstddef>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "hsc/petri/core/SparsePetriNet.h"
#include "hsc/petri/expr/Property.h"
#include "hsc/petri/expr/Simplify.h"
#include "hsc/petri/props_to_surface.hh"
#include "hsc/surface/sexpr.hh"
#include "hsc/surface/translate.hh"

namespace hsc::pn {

constexpr const char* TECHNIQUES = " TECHNIQUES DECISION_DIAGRAMS SATURATION";
constexpr const char* TRIVIAL = " TECHNIQUES TOPOLOGICAL TRIVIAL";
constexpr const char* APPROX = " TECHNIQUES DECISION_DIAGRAMS TOPOLOGICAL";  ///< refuted on the invariant set

/// Owns the session and its output buffer; every query is a fresh batch.
class solver {
 public:
  /// \p bound is the effective leaf domain size: every place lies in
  /// [0, bound). \p verbose forwards the session's other lines to stderr.
  solver(const SparsePetriNet<int>& net, int bound, bool verbose)
      : net_(net), bound_(bound), verbose_(verbose), session_(buf_) {}

  /// How many transitions of the producer's original net each transition
  /// stands for (PNET's `TMULT`; all ones when the net carries no block).
  /// Arc counts are multiplied by it, so an unfolded or reduced net reports
  /// the arcs of the net it came from.
  void set_multiplicities(std::vector<long long> mult) {
    mult_ = std::move(mult);
  }

  /// Whether an arc count over this net is the arc count of the net the
  /// caller cares about. False when the net reached us transformed by steps
  /// that did not account for what they dropped, in which case the arcs are
  /// simply not reported rather than reported low.
  void set_arcs_countable(bool countable) { arcs_countable_ = countable; }
  /// Forward the witness tree of every CTL verdict to stderr (`--witness`).
  void set_witness(bool on) { witness_ = on; }

  /// What constant places removed by the producer held (PNET's `PDROP`).
  /// Those tokens sit in every marking, so they add to the per-marking total,
  /// and each value is a candidate for the largest marking of a place.
  void set_dropped_tokens(std::vector<long long> held) {
    dropped_ = std::move(held);
  }

  /// A deadline for the questions that follow: a CTL property still running
  /// at that instant answers `TIMEOUT` and is left open; `nullopt` removes
  /// it. Reachability questions are single applications and run to their end.
  void set_deadline(std::optional<std::chrono::steady_clock::time_point> at) {
    session_.set_deadline(at);
  }

  /// Feed a batch of forms given as text; returns the lines it produced.
  std::vector<std::string> feed(const std::string& text) {
    session_.feed(hsc::surface::parse(text));
    std::vector<std::string> lines;
    std::string line;
    std::istringstream in(buf_.str());
    while (std::getline(in, line)) lines.push_back(line);
    buf_.str("");
    buf_.clear();
    return lines;
  }

  /// `(select Q R ATOM) (count Q)`: is the selection non-empty? With
  /// \p witness_for (a property name) and `--witness`, a non-empty
  /// selection is followed by a shortest path to it on stderr.
  bool nonempty(const std::string& atom, const std::string* witness_for = nullptr) {
    const std::string q = next_name();
    const bool some = value_of(feed("(select " + q + " R " + atom + ") (count " + q + ")"),
                               q + " count ") != "0";
    if (some && witness_ && witness_for != nullptr) witness_path(*witness_for, q);
    return some;
  }

  /// `(select Q SET ATOM) (count Q)`: is the selection of a named set non-empty?
  bool nonempty_on(const std::string& set, const std::string& atom) {
    const std::string q = next_name();
    return value_of(feed("(select " + q + " " + set + " " + atom + ") (count " + q + ")"), q + " count ") != "0";
  }

  /// Whether \p e reads a place \p bound only caps (a negative entry).
  static bool reads_capped(const ::petri::expr::Expression& e, const std::vector<long long>& bound) {
    if (e.kind == ::petri::expr::Expression::Kind::Atom) {
      for (const auto& [p, c] : e.atom.terms)
        if (p < bound.size() && bound[p] < 0) return true;
      return false;
    }
    for (const ::petri::expr::Expression& k : e.children)
      if (reads_capped(k, bound)) return true;
    return false;
  }

  /// Refute \p p on the over-approximation named \p set (`S ⊇ R`, exact on the
  /// places \p bound gives a bound for): a reachability goal that selects
  /// nothing of it is FALSE, an invariant whose negation selects nothing is
  /// TRUE, a deadlock the set has no marking for is FALSE (a dead marking
  /// stays dead when its capped places are clipped, so caps do not matter
  /// there). A goal that reads a capped place, or that the set does not rule
  /// out, stays open: false is returned and nothing is printed.
  bool refute(const ::petri::expr::Property& p, const std::string& set,
              const std::vector<long long>& bound, std::ostream& out) {
    using ::petri::expr::Expression;
    using ::petri::expr::PropertyKind;
    const std::vector<std::string>& pnames = net_.getPnames();
    switch (p.kind) {
      case PropertyKind::Reachability:
      case PropertyKind::Invariant: {
        const bool inv = p.kind == PropertyKind::Invariant;
        const Expression goal = ::petri::expr::simplify(inv ? Expression::makeNot(p.body) : p.body);
        if (goal.isConstant()) return false;  // the exact pass answers it trivially
        if (reads_capped(goal, bound)) return false;
        if (nonempty_on(set, hsc::petri::query_atom(goal, pnames))) return false;
        out << "FORMULA " << p.name << ' ' << verdict(inv, false) << APPROX << std::endl;
        return true;
      }
      case PropertyKind::Deadlock: {
        const std::optional<std::string> atom = hsc::petri::deadlock_atom(net_);
        if (!atom || nonempty_on(set, *atom)) return false;
        out << "FORMULA " << p.name << " FALSE" << APPROX << std::endl;
        return true;
      }
      default:
        return false;
    }
  }

  /// `WITNESS <name> path K` then the run, on stderr: a shortest path from
  /// the initial marking (named once as a word) to the selection \p q.
  void witness_path(const std::string& name, const std::string& q) {
    if (!seed_named_) {
      const std::vector<std::string>& pnames = net_.getPnames();
      const std::vector<int>& marks = net_.getMarks();
      std::string w = "(word hsc-pn-I";
      for (std::size_t i = 0; i < pnames.size(); ++i) {
        if (marks[i] != 0) w += " (" + pnames[i] + ' ' + std::to_string(marks[i]) + ')';
      }
      feed(w + ")");
      seed_named_ = true;
    }
    const std::string w = next_name();
    for (const std::string& l : feed("(path " + w + " hsc-pn-I " + q + ")")) {
      if (l.rfind(w + " path", 0) == 0) {
        std::cerr << "WITNESS " << name << l.substr(w.size()) << '\n';
      } else {
        std::cerr << without_zero_places(l) << '\n';
      }
    }
  }

  /// `(count Q exact)` of a selection (`R` itself when \p atom is empty).
  std::string exact_count(const std::optional<std::string>& atom) {
    if (!atom) return value_of(feed("(count R exact)"), "R count ");
    const std::string q = next_name();
    return value_of(feed("(select " + q + " R " + *atom + ") (count " + q + " exact)"),
                    q + " count ");
  }

  /// The maximum of \p form over the reachable states: `(max-sum R terms)`,
  /// one pass over the diagram, exact. The hint (an UpperBounds hint) is not
  /// needed and only checked against the result in verbose mode.
  long long maximum(const ::petri::expr::LinearAtom& form, long long hint) {
    const std::vector<std::string>& pnames = net_.getPnames();
    std::string q = "(max-sum R";
    for (const auto& [place, coeff] : form.terms) q += " (* " + std::to_string(coeff) + ' ' + pnames[place] + ')';
    const std::string v = value_of(feed(q + ")"), "R max-sum ");
    const long long m = v == "none" ? 0 : std::stoll(v);
    if (verbose_ && hint >= 0 && hint != m) std::cerr << "bound hint " << hint << " against the maximum " << m << '\n';
    return m;
  }

  /// Answer one property with a FORMULA line; false when it is left open.
  bool answer(const ::petri::expr::Property& p, std::ostream& out) {
    using ::petri::expr::Expression;
    using ::petri::expr::PropertyKind;
    const std::vector<std::string>& pnames = net_.getPnames();
    switch (p.kind) {
      case PropertyKind::Reachability:
      case PropertyKind::Invariant: {
        const bool inv = p.kind == PropertyKind::Invariant;
        const Expression goal = ::petri::expr::simplify(
            inv ? Expression::makeNot(p.body) : p.body);
        if (goal.isConstant()) {
          const bool reached = goal.kind == Expression::Kind::True;
          out << "FORMULA " << p.name << ' ' << verdict(inv, reached) << TRIVIAL
              << std::endl;
          return true;
        }
        const bool reached = nonempty(hsc::petri::query_atom(goal, pnames), &p.name);
        out << "FORMULA " << p.name << ' ' << verdict(inv, reached) << TECHNIQUES
            << std::endl;
        return true;
      }
      case PropertyKind::Deadlock: {
        const std::optional<std::string> atom = hsc::petri::deadlock_atom(net_);
        if (!atom) {
          out << "FORMULA " << p.name << " FALSE" << TRIVIAL << std::endl;
          return true;
        }
        out << "FORMULA " << p.name << ' ' << (nonempty(*atom, &p.name) ? "TRUE" : "FALSE")
            << TECHNIQUES << std::endl;
        return true;
      }
      case PropertyKind::Bound:
        out << "FORMULA " << p.name << ' ' << maximum(p.body.atom, p.boundHint)
            << TECHNIQUES << std::endl;
        return true;
      case PropertyKind::CTL: {
        // `(ctl Q FORMULA)` answers `Q ctl TRUE|FALSE|UNKNOWN`; unknown is
        // left open (no line), never guessed.
        const std::string q = next_name();
        const std::string v = value_of(
            feed("(ctl " + q + " " + hsc::petri::ctl_text(p.ctl, pnames) + ")"),
            q + " ctl ");
        if (v != "TRUE" && v != "FALSE") return false;
        out << "FORMULA " << p.name << ' ' << v << TECHNIQUES << std::endl;
        if (witness_) {
          for (const std::string& l : feed("(witness " + q + ")")) {
            if (l.rfind(q + " witness", 0) == 0) {
              std::cerr << "WITNESS " << p.name << l.substr(q.size() + 8) << '\n';
            } else {
              std::cerr << without_zero_places(l) << '\n';
            }
          }
        }
        return true;
      }
      case PropertyKind::Unsupported:
        return false;
    }
    return false;
  }

  /// A word line `((p 0) (q 2) …)` without its `(name 0)` pairs; other
  /// lines unchanged.
  static std::string without_zero_places(const std::string& line) {
    const std::size_t open = line.find("((");
    if (open == std::string::npos) return line;
    std::string out = line.substr(0, open + 1);
    std::size_t i = open + 1;
    while (i < line.size() && line[i] == '(') {
      const std::size_t close = line.find(')', i);
      if (close == std::string::npos) return line;
      const std::string pair = line.substr(i, close - i + 1);
      if (pair.size() < 3 || pair.compare(pair.size() - 3, 3, " 0)") != 0) {
        if (out.back() != '(') out += ' ';
        out += pair;
      }
      i = close + 1;
      while (i < line.size() && line[i] == ' ') ++i;
    }
    return out + line.substr(i);
  }

  /// `STATE_SPACE MAX_TOKEN_IN_PLACE`: the largest marking of any place,
  /// the removed constant places among them.
  void max_tokens(std::ostream& out) {
    long long most = std::stoll(value_of(feed("(max-value R)"), "R max-value "));
    for (const long long held : dropped_) most = std::max(most, held);
    out << "STATE_SPACE MAX_TOKEN_IN_PLACE " << most << TECHNIQUES << std::endl;
  }

  /// The four lines of the MCC StateSpace examination.
  void state_space(std::ostream& out) {
    out << "STATE_SPACE STATES " << exact_count(std::nullopt) << TECHNIQUES
        << std::endl;
    max_tokens(out);
    long long constant = 0;
    for (const long long held : dropped_) constant += held;
    const std::string v = value_of(feed("(max-sum R)"), "R max-sum ");
    out << "STATE_SPACE MAX_TOKEN_PER_MARKING " << (v == "none" ? 0 : std::stoll(v)) + constant
        << TECHNIQUES << std::endl;
    if (!arcs_countable_) {
      std::cerr << "TRANSITIONS not reported: this net carries no evidence "
                   "that its arcs are those of the net it came from\n";
      return;
    }
    // arcs of the reachability graph: per transition, the states enabling it,
    // weighted by what that transition stands for (TMULT)
    mpz_class edges = 0;
    for (std::size_t t = 0; t < net_.getTransitionCount(); ++t) {
      const mpz_class states(exact_count(hsc::petri::guard_atom(net_, t)));
      edges += mpz_class(static_cast<long>(multiplicity(t))) * states;
    }
    out << "STATE_SPACE TRANSITIONS " << edges << TECHNIQUES << std::endl;
  }

 private:
  static const char* verdict(bool invariant, bool reached) {
    return invariant ? (reached ? "FALSE" : "TRUE") : (reached ? "TRUE" : "FALSE");
  }

  std::string next_name() { return "hsc-pn-q" + std::to_string(++counter_); }

  [[nodiscard]] long long multiplicity(std::size_t t) const {
    return t < mult_.size() ? mult_[t] : 1;
  }

  /// The value after \p prefix on the line that starts with it; other lines
  /// are the session's own reports (rewrite traces, ok/FAIL), forwarded when
  /// verbose. A missing line is a protocol error.
  std::string value_of(const std::vector<std::string>& lines,
                       const std::string& prefix) {
    std::optional<std::string> found;
    for (const std::string& l : lines) {
      if (l.compare(0, prefix.size(), prefix) == 0) found = l.substr(prefix.size());
      else if (verbose_) std::cerr << l << '\n';
    }
    if (!found) throw std::runtime_error("no `" + prefix + "` line in the session output");
    return *found;
  }

  const SparsePetriNet<int>& net_;
  int bound_;
  bool verbose_;
  bool witness_ = false;    ///< forward witnesses to stderr
  bool seed_named_ = false; ///< `(word hsc-pn-I …)` fed, for the paths
  std::vector<long long> mult_;  ///< empty means every multiplicity is 1
  bool arcs_countable_ = true;
  std::vector<long long> dropped_;  ///< markings of removed constant places
  std::ostringstream buf_;
  hsc::surface::session session_;
  std::size_t counter_ = 0;
};

}  // namespace hsc::pn
