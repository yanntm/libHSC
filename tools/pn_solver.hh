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
constexpr const char* APPROX_BACK = " TECHNIQUES DECISION_DIAGRAMS TOPOLOGICAL K_INDUCTION";  ///< decided by the backward search in the invariant set

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

  /// The place names the properties' indices refer to, when the properties
  /// were parsed against another net than the session's (an abstraction
  /// keeps the names): used wherever a goal is spelled out.
  void set_property_names(const std::vector<std::string>& names) { prop_names_ = &names; }

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
  /// Whether the goal of \p p reads a place \p bound marks removed or capped.
  static bool reads_removed(const ::petri::expr::Property& p, const std::vector<long long>& bound) {
    using ::petri::expr::PropertyKind;
    if (p.kind != PropertyKind::Reachability && p.kind != PropertyKind::Invariant) return false;
    return reads_capped(p.body, bound);
  }

  /// The tests that ran out of their time (`refute` with a deadline).
  [[nodiscard]] std::size_t timeouts() const { return timeouts_; }

  /// `refute` under a deadline of \p seconds (0: none): a test that runs
  /// out leaves the property open and counts as a timeout.
  bool refute(const ::petri::expr::Property& p, const std::string& set,
              const std::vector<long long>& bound, std::ostream& out, double seconds) {
    if (seconds <= 0) return refute(p, set, bound, out);
    set_deadline(std::chrono::steady_clock::now() +
                 std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds)));
    bool r = false;
    try {
      r = refute(p, set, bound, out);
    } catch (const std::exception&) {  // the selection was cut: no `count` line came back
      ++timeouts_;
    }
    set_deadline(std::nullopt);
    return r;
  }

  bool refute(const ::petri::expr::Property& p, const std::string& set,
              const std::vector<long long>& bound, std::ostream& out) {
    using ::petri::expr::Expression;
    using ::petri::expr::PropertyKind;
    const std::vector<std::string>& pnames = property_names();
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

  /// The names of the places \p e reads.
  static void places_of(const ::petri::expr::Expression& e, const std::vector<std::string>& pnames, std::string& out) {
    if (e.kind == ::petri::expr::Expression::Kind::Atom) {
      for (const auto& [p, c] : e.atom.terms) if (p < pnames.size()) out += ' ' + pnames[p];
      return;
    }
    for (const ::petri::expr::Expression& k : e.children) places_of(k, pnames, out);
  }

  /// Decide \p p by a backward search inside the over-approximation \p set:
  /// `(backward B X set steps K writing …)` from `X`, the goal's markings of
  /// the set. A layer meeting the initial marking is a path of the session's
  /// net: reachable (TRUE for a reachability, FALSE for an invariant) — a
  /// verdict taken only with \p exact_net, when that net is the original one
  /// (an abstraction has more behaviour: its paths need not exist). A closed
  /// search with nothing left is unreachable — sound only when every place
  /// is exact in the set (\p all_exact), since a capped predecessor is
  /// missed. Open or partial searches, goals reading capped places,
  /// constants and other kinds leave the property open (false returned).
  bool refute_back(const ::petri::expr::Property& p, const std::string& set,
                   const std::vector<long long>& bound, bool all_exact, bool exact_net, std::size_t steps,
                   std::ostream& out, std::string* how = nullptr) {
    using ::petri::expr::Expression;
    using ::petri::expr::PropertyKind;
    if (p.kind != PropertyKind::Reachability && p.kind != PropertyKind::Invariant) return false;
    const bool inv = p.kind == PropertyKind::Invariant;
    const Expression goal = ::petri::expr::simplify(inv ? Expression::makeNot(p.body) : p.body);
    if (goal.isConstant() || reads_capped(goal, bound)) return false;
    const std::vector<std::string>& pnames = property_names();
    std::string places;
    places_of(goal, pnames, places);
    const std::string x = next_name(), b = next_name();
    std::string v;
    try {
      v = value_of(
          feed("(select " + x + " " + set + " " + hsc::petri::query_atom(goal, pnames) + ") (backward " + b + " " + x +
               " " + set + " steps " + std::to_string(steps) + " writing" + places + ")"),
          b + " backward ");
    } catch (const std::exception&) {  // the selection itself was cut by the deadline
      ++timeouts_;
      v = "partial 0";
    }
    if (how != nullptr) *how = v;
    const std::string kind = v.substr(0, v.find(' '));
    if (kind == "init" && exact_net) {
      out << "FORMULA " << p.name << ' ' << verdict(inv, true) << APPROX_BACK << std::endl;
      return true;
    }
    if (kind == "closed" && all_exact) {
      out << "FORMULA " << p.name << ' ' << verdict(inv, false) << APPROX_BACK << std::endl;
      return true;
    }
    return false;
  }

  /// `WITNESS <name> path K` then the run, on stderr: a shortest path from
  /// the initial marking (named once as a word) to the selection \p q.
  void witness_path(const std::string& name, const std::string& q) {
    if (!seed_named_) {
      const std::vector<std::string>& pnames = property_names();
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
    const std::vector<std::string>& pnames = property_names();
    std::string q = "(max-sum R";
    for (const auto& [place, coeff] : form.terms) q += " (* " + std::to_string(coeff) + ' ' + pnames[place] + ')';
    const std::string v = value_of(feed(q + ")"), "R max-sum ");
    const long long m = v == "none" ? 0 : std::stoll(v);
    if (verbose_ && hint >= 0 && hint != m) std::cerr << "bound hint " << hint << " against the maximum " << m << '\n';
    return m;
  }

  /// Answer one property with a FORMULA line; false when it is left open.
  /// With \p partial the session's `R` is an under-approximation (its
  /// closure was cut): every state in it is reachable and nothing says the
  /// rest is not, so only a goal met in it is answered (a reachable goal,
  /// a violated invariant, a deadlock found); an empty selection, a bound,
  /// and a CTL verdict the session itself does not stand by are left open.
  bool answer(const ::petri::expr::Property& p, std::ostream& out, bool partial = false) {
    try {
      return answer_now(p, out, partial);
    } catch (const hsc::interrupted&) {
      // The deadline met inside the question (a selection, a closure, the
      // model of the checker): the property stays open, the session is
      // whole (every batch is fresh, a form that stopped bound nothing).
      ++timeouts_;
      if (verbose_) std::cerr << "hsc-pn: " << p.name << " cut by the deadline\n";
      return false;
    }
  }

  bool answer_now(const ::petri::expr::Property& p, std::ostream& out, bool partial) {
    using ::petri::expr::Expression;
    using ::petri::expr::PropertyKind;
    const std::vector<std::string>& pnames = property_names();
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
        if (partial && !reached) return false;
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
        const bool found = nonempty(*atom, &p.name);
        if (partial && !found) return false;
        out << "FORMULA " << p.name << ' ' << (found ? "TRUE" : "FALSE")
            << TECHNIQUES << std::endl;
        return true;
      }
      case PropertyKind::Bound:
        if (partial) return false;  // a maximum over part of the set is a lower bound
        out << "FORMULA " << p.name << ' ' << maximum(p.body.atom, p.boundHint)
            << TECHNIQUES << std::endl;
        return true;
      case PropertyKind::CTL: {
        // `(ctl Q FORMULA)` answers `Q ctl TRUE|FALSE|UNKNOWN`; unknown is
        // left open (no line), never guessed. On a partial set the session
        // keeps only the verdicts that stand (`ctl/algorithm.md` §7).
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

  const std::vector<std::string>& property_names() const { return prop_names_ != nullptr ? *prop_names_ : net_.getPnames(); }

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
  const std::vector<std::string>* prop_names_ = nullptr;  ///< see set_property_names
  std::size_t timeouts_ = 0;  ///< see timeouts()
  std::vector<long long> mult_;  ///< empty means every multiplicity is 1
  bool arcs_countable_ = true;
  std::vector<long long> dropped_;  ///< markings of removed constant places
  std::ostringstream buf_;
  hsc::surface::session session_;
  std::size_t counter_ = 0;
};

}  // namespace hsc::pn
