/// \file surface_expand.cc
/// \brief The parametric pass (datum → datum): compile-time parameters,
/// count-form arrays, and the binders `forall` / `exists`.
///
/// A binder generates a list in place; the position of that list decides its
/// meaning. Each syntactic context has a native combinator: `forall` expands
/// via the conjunctive one (seq / and / splice into a conjunctive list),
/// `exists` via the disjunctive one (alt / or / splice into a disjunctive
/// list), realized as a splice when the context's combinator already
/// matches, a wrapped node when it does not, and an event *family* (one
/// plain event per index value, `NAME_v`) when the choice spans event
/// clauses. Substitution grounds binder indexes to integer atoms; constant
/// arithmetic folds bottom-up, so `(% (+ i 1) N)` becomes a literal and a
/// grounded access to a generated array becomes its cell name. The
/// context → combinator table is in research_notes/hsc_parametric.md.

#include "hsc/surface/expand.hh"

#include <map>
#include <optional>
#include <set>
#include <string>

namespace hsc::surface {

namespace {

bool is_int_text(const std::string& t) {
  if (t.empty()) return false;
  std::size_t i = (t[0] == '-') ? 1 : 0;
  if (i == t.size()) return false;
  for (; i < t.size(); ++i)
    if (t[i] < '0' || t[i] > '9') return false;
  return true;
}

bool is_int_atom(const datum& d) { return d.is_atom() && is_int_text(d.text()); }

long long int_value(const datum& d) { return std::stoll(d.text()); }

datum int_atom(long long v, int line) {
  return datum::atom(std::to_string(v), line);
}

datum list_of(std::string head, std::vector<datum> items, int line) {
  std::vector<datum> all;
  all.reserve(items.size() + 1);
  all.push_back(datum::atom(std::move(head), line));
  for (datum& d : items) all.push_back(std::move(d));
  return datum::list(std::move(all), line);
}

bool is_arith_op(const std::string& h) {
  return h == "+" || h == "-" || h == "*" || h == "/" || h == "%" ||
         h == "<<" || h == ">>" || h == "&" || h == "|" || h == "^" ||
         h == "~";
}

bool is_bool_head(const std::string& h) {
  return h == "and" || h == "or" || h == "not" || h == "imply" || h == "==" ||
         h == "!=" || h == "<" || h == "<=" || h == ">" || h == ">=" ||
         h == "in";
}

bool is_evterm_head(const std::string& h) {
  return h == "when" || h == "do" || h == "alt" || h == "seq";
}

bool is_binder_head(const std::string& h) {
  return h == "forall" || h == "exists";
}

/// A parsed binder head: `(forall (I HI) …)` or `(forall (I LO HI) …)`,
/// range [lo, hi). The body is items[2..] of the binder form.
struct binder {
  std::string name;
  long long lo = 0;
  long long hi = 0;
  int line = 0;
};

/// One alternative of an event body under expansion: the name suffix
/// accumulated from `exists` index values, and the clause list.
struct alt_branch {
  std::string suffix;
  std::vector<datum> clauses;
};
using branches = std::vector<alt_branch>;

branches cross(const branches& a, const branches& b) {
  branches out;
  out.reserve(a.size() * b.size());
  for (const alt_branch& x : a)
    for (const alt_branch& y : b) {
      alt_branch c{x.suffix + y.suffix, x.clauses};
      c.clauses.insert(c.clauses.end(), y.clauses.begin(), y.clauses.end());
      out.push_back(std::move(c));
    }
  return out;
}

class expander {
 public:
  explicit expander(bool families) : families_(families) {}

  std::vector<datum> run(const std::vector<datum>& forms) {
    collect_names(forms);
    std::vector<datum> out;
    top_forms(forms, out);
    return out;
  }

 private:
  bool families_;                            // emit certified family forms
  std::map<std::string, long long> env_;     // params and bound indexes
  std::map<std::string, long long> arrays_;  // count-form arrays: name → size
  // 2-D count-form arrays: name → {rows, cols}; cells NAME_i_j, row-major
  std::map<std::string, std::pair<long long, long long>> arrays2_;
  std::set<std::string> names_;              // every declared or bound name

  // ---- names and environment ----------------------------------------

  /// Pre-scan the NAME slot of every declaring form, so a later `param` or
  /// binder cannot capture a model name declared anywhere in the file.
  void collect_names(const std::vector<datum>& forms) {
    for (const datum& f : forms) {
      const std::string& h = f.head();
      if (h == "leaf" || h == "array" || h == "event" || h == "alt" ||
          h == "seq" || h == "word" || h == "reach" || h == "apply" ||
          h == "select") {
        if (f.items().size() >= 2 && f.items()[1].is_atom())
          names_.insert(f.items()[1].text());
      } else if (is_binder_head(h)) {
        // names declared under a top-level binder still count
        if (f.items().size() >= 3)
          collect_names({f.items().begin() + 2, f.items().end()});
      }
    }
  }

  void bind(const binder& b) {
    if (env_.count(b.name))
      throw expand_error(b.line, "'" + b.name + "' shadows an enclosing "
                                 "binder or param");
    if (names_.count(b.name))
      throw expand_error(b.line, "'" + b.name + "' collides with a "
                                 "declared name");
    env_[b.name] = 0;
  }
  void set(const binder& b, long long v) { env_[b.name] = v; }
  void unbind(const binder& b) { env_.erase(b.name); }

  binder parse_binder(const datum& d) {
    const std::vector<datum>& it = d.items();
    if (it.size() < 3 || !it[1].is_list() || it[1].items().size() < 2 ||
        it[1].items().size() > 3 || !it[1].items()[0].is_atom() ||
        is_int_atom(it[1].items()[0]))
      throw expand_error(d.line(), d.head() + " expects (NAME HI) or "
                                   "(NAME LO HI) then a body");
    binder b;
    b.line = d.line();
    b.name = it[1].items()[0].text();
    if (it[1].items().size() == 2) {
      b.hi = eval_int(it[1].items()[1]);
    } else {
      b.lo = eval_int(it[1].items()[1]);
      b.hi = eval_int(it[1].items()[2]);
    }
    return b;
  }

  long long eval_int(const datum& d) {
    const datum v = expr_node(d);
    if (!is_int_atom(v))
      throw expand_error(d.line(), "does not fold to an integer constant");
    return int_value(v);
  }

  // ---- expressions ----------------------------------------------------

  /// EXPR position: substitute in-scope names, fold constant arithmetic,
  /// rewrite a grounded access to a generated array to its cell atom.
  datum expr_node(const datum& d) {
    if (d.is_atom()) {
      const auto it = env_.find(d.text());
      if (it != env_.end()) return int_atom(it->second, d.line());
      return d;
    }
    const std::string& h = d.head();
    if (is_binder_head(h))
      throw expand_error(d.line(), "a binder cannot stand in an integer "
                                   "expression");
    if (h == "at") return at_node(d);
    if (is_bool_head(h)) return bexp_node(d);  // 0/1 coercion position
    std::vector<datum> kids;
    for (std::size_t i = 1; i < d.items().size(); ++i)
      kids.push_back(expr_node(d.items()[i]));
    if (is_arith_op(h)) {
      if (h == "~" && kids.size() == 1 && is_int_atom(kids[0]))
        return int_atom(~int_value(kids[0]), d.line());
      if (kids.size() == 2 && is_int_atom(kids[0]) && is_int_atom(kids[1]))
        return fold(h, int_value(kids[0]), int_value(kids[1]), d.line());
    }
    return list_of(h, std::move(kids), d.line());
  }

  datum fold(const std::string& op, long long a, long long b, int line) {
    if ((op == "/" || op == "%") && b == 0)
      throw expand_error(line, "division by zero while folding constants");
    long long r = 0;
    if (op == "+") r = a + b;
    else if (op == "-") r = a - b;
    else if (op == "*") r = a * b;
    else if (op == "/") r = a / b;
    else if (op == "%") r = a % b;
    else if (op == "<<") r = a << b;
    else if (op == ">>") r = a >> b;
    else if (op == "&") r = a & b;
    else if (op == "|") r = a | b;
    else if (op == "^") r = a ^ b;
    return int_atom(r, line);
  }

  datum at_node(const datum& d) {
    if ((d.items().size() != 3 && d.items().size() != 4) ||
        !d.items()[1].is_atom())
      throw expand_error(d.line(), "at expects (at NAME EXPR [EXPR])");
    const std::string& name = d.items()[1].text();
    datum idx = expr_node(d.items()[2]);
    if (d.items().size() == 4) {  // 2-D access
      const auto it2 = arrays2_.find(name);
      if (it2 == arrays2_.end())
        throw expand_error(d.line(), "'" + name + "' is not a 2-D array");
      const auto [rows, cols] = it2->second;
      datum jdx = expr_node(d.items()[3]);
      if (is_int_atom(idx) && is_int_atom(jdx)) {
        const long long i = int_value(idx), j = int_value(jdx);
        if (i < 0 || i >= rows || j < 0 || j >= cols)
          throw expand_error(d.line(), "index (" + std::to_string(i) + " " +
                                       std::to_string(j) +
                                       ") out of range for array '" + name +
                                       "' of " + std::to_string(rows) + "x" +
                                       std::to_string(cols));
        return datum::atom(
            name + "_" + std::to_string(i) + "_" + std::to_string(j),
            d.line());
      }
      // open index: row-major linearization onto the flat cell list
      datum lin = expr_node(list_of(
          "+",
          {list_of("*", {std::move(idx), int_atom(cols, d.line())}, d.line()),
           std::move(jdx)},
          d.line()));
      return list_of("at", {d.items()[1], std::move(lin)}, d.line());
    }
    const auto it = arrays_.find(name);
    if (it != arrays_.end() && is_int_atom(idx)) {
      const long long k = int_value(idx);
      if (k < 0 || k >= it->second)
        throw expand_error(d.line(), "index " + std::to_string(k) +
                                     " out of range for array '" + name +
                                     "' of size " + std::to_string(it->second));
      return datum::atom(name + "_" + std::to_string(k), d.line());
    }
    const auto it2 = arrays2_.find(name);
    if (it2 != arrays2_.end() && is_int_atom(idx)) {
      // grounded flat access to a 2-D array: the row-major cell
      const long long k = int_value(idx);
      const auto [rows, cols] = it2->second;
      if (k < 0 || k >= rows * cols)
        throw expand_error(d.line(), "flat index " + std::to_string(k) +
                                     " out of range for array '" + name +
                                     "' of " + std::to_string(rows) + "x" +
                                     std::to_string(cols));
      return datum::atom(name + "_" + std::to_string(k / cols) + "_" +
                             std::to_string(k % cols),
                         d.line());
    }
    return list_of("at", {d.items()[1], std::move(idx)}, d.line());
  }

  // ---- boolean expressions --------------------------------------------

  /// A conjunctive (`conj`) or disjunctive BEXP list: the body of `when`,
  /// `and`, a select's QATOMs (conj) or of `or` (disj). A binder of the
  /// matching flavor splices; the opposite flavor wraps. A binder body of
  /// several datums is a per-instance conjunction, as in `when`.
  std::vector<datum> bexp_list(const datum* first, const datum* last,
                               bool conj) {
    std::vector<datum> out;
    for (const datum* it = first; it != last; ++it) {
      const std::string& h = it->head();
      if (!is_binder_head(h)) {
        out.push_back(bexp_node(*it));
        continue;
      }
      const bool splice = (h == "forall") == conj;
      binder b = parse_binder(*it);
      bind(b);
      std::vector<datum> parts;
      for (long long v = b.lo; v < b.hi; ++v) {
        set(b, v);
        std::vector<datum> inst =
            bexp_list(&*(it->items().begin() + 2),
                      &*(it->items().begin() + 2) + (it->items().size() - 2),
                      /*conj=*/true);
        if (splice && conj) {
          for (datum& p : inst) parts.push_back(std::move(p));
        } else {
          parts.push_back(inst.size() == 1
                              ? std::move(inst[0])
                              : list_of("and", std::move(inst), b.line));
        }
      }
      unbind(b);
      if (splice) {
        for (datum& p : parts) out.push_back(std::move(p));
      } else {
        out.push_back(list_of(h == "forall" ? "and" : "or", std::move(parts),
                              b.line));
      }
    }
    return out;
  }

  datum bexp_node(const datum& d) {
    if (d.is_atom()) return expr_node(d);
    const std::string& h = d.head();
    const datum* body = d.items().data() + 1;
    const datum* end = d.items().data() + d.items().size();
    if (h == "and") return list_of("and", bexp_list(body, end, true), d.line());
    if (h == "or") return list_of("or", bexp_list(body, end, false), d.line());
    if (h == "not" || h == "imply") {
      std::vector<datum> kids;
      for (const datum* it = body; it != end; ++it)
        kids.push_back(bexp_node(*it));
      return list_of(h, std::move(kids), d.line());
    }
    if (is_binder_head(h)) {
      // single BEXP slot: wrap in the binder's flavor
      std::vector<datum> one = bexp_list(&d, &d + 1, /*conj=*/h == "exists");
      return one.size() == 1 ? std::move(one[0])
                             : list_of(h == "forall" ? "and" : "or",
                                       std::move(one), d.line());
    }
    // comparison, `in`, or an integer form in boolean position
    return expr_node_children(d);
  }

  datum expr_node_children(const datum& d) {
    std::vector<datum> kids;
    for (std::size_t i = 1; i < d.items().size(); ++i)
      kids.push_back(expr_node(d.items()[i]));
    return list_of(d.head(), std::move(kids), d.line());
  }

  // ---- actions ---------------------------------------------------------

  /// The body of one `do` clause. `forall` splices — the clause stays one
  /// simultaneous multi-assign. `exists` has no combinator inside a clause.
  std::vector<datum> act_list(const datum* first, const datum* last) {
    std::vector<datum> out;
    for (const datum* it = first; it != last; ++it) {
      const std::string& h = it->head();
      if (h == "exists")
        throw expand_error(it->line(),
                           "choice inside one clause: lift the exists to "
                           "clause level, as (exists (i N) (do …))");
      if (h == "forall") {
        binder b = parse_binder(*it);
        bind(b);
        for (long long v = b.lo; v < b.hi; ++v) {
          set(b, v);
          std::vector<datum> inst =
              act_list(it->items().data() + 2,
                       it->items().data() + it->items().size());
          for (datum& a : inst) out.push_back(std::move(a));
        }
        unbind(b);
        continue;
      }
      out.push_back(act_node(*it));
    }
    return out;
  }

  datum act_node(const datum& d) {
    if (d.is_atom() || d.items().empty()) return d;  // translator reports
    const std::string& h = d.head();
    if (h == ":=" || h == "+=" || h == "-=" || h == "havoc") {
      if (d.items().size() >= 2 && d.items()[1].is_atom() &&
          env_.count(d.items()[1].text()))
        throw expand_error(d.line(), "a binder index cannot be assigned");
      return expr_node_children(d);
    }
    return expr_node_children(d);
  }

  // ---- event terms -----------------------------------------------------

  datum evterm_node(const datum& d) {
    if (d.is_atom()) return d;  // a declared term's name
    const std::string& h = d.head();
    const datum* body = d.items().data() + 1;
    const datum* end = d.items().data() + d.items().size();
    if (h == "when")
      return list_of("when", bexp_list(body, end, true), d.line());
    if (h == "do") return list_of("do", act_list(body, end), d.line());
    if (h == "alt")
      return list_of("alt", evterm_list(body, end, /*seq=*/false), d.line());
    if (h == "seq")
      return list_of("seq", evterm_list(body, end, /*seq=*/true), d.line());
    if (is_binder_head(h)) {
      std::vector<datum> parts = evterm_list(&d, &d + 1, /*seq=*/h == "forall");
      if (parts.size() == 1) return std::move(parts[0]);
      return list_of(h == "forall" ? "seq" : "alt", std::move(parts),
                     d.line());
    }
    return d;  // unknown term form: the translator reports
  }

  /// An EVTERM list under a `seq` (seq=true) or `alt` (seq=false) parent.
  /// The matching binder flavor splices its instances; the opposite one
  /// wraps each expansion in a node of its own combinator. A binder body of
  /// several datums is one sequential instance (a guarded command).
  std::vector<datum> evterm_list(const datum* first, const datum* last,
                                 bool seq) {
    std::vector<datum> out;
    for (const datum* it = first; it != last; ++it) {
      const std::string& h = it->head();
      if (!is_binder_head(h)) {
        out.push_back(evterm_node(*it));
        continue;
      }
      binder b = parse_binder(*it);
      const bool splice = (h == "forall") == seq;
      bind(b);
      std::vector<datum> parts;
      for (long long v = b.lo; v < b.hi; ++v) {
        set(b, v);
        std::vector<datum> inst = evterm_list(
            it->items().data() + 2, it->items().data() + it->items().size(),
            /*seq=*/true);
        if (h == "forall") {
          for (datum& p : inst) parts.push_back(std::move(p));
        } else {
          parts.push_back(inst.size() == 1
                              ? std::move(inst[0])
                              : list_of("seq", std::move(inst), b.line));
        }
      }
      unbind(b);
      if (parts.empty()) {
        // empty forall: the identity filter; empty exists: the zero term
        parts.push_back(h == "forall" ? list_of("when", {}, b.line)
                                      : list_of("abort", {}, b.line));
      }
      if (splice) {
        for (datum& p : parts) out.push_back(std::move(p));
      } else {
        out.push_back(parts.size() == 1
                          ? std::move(parts[0])
                          : list_of(h == "forall" ? "seq" : "alt",
                                    std::move(parts), b.line));
      }
    }
    return out;
  }

  // ---- certified uniform families (spec Part II §6) --------------------

  /// The offset a certified index expression puts on the binder: `i` is 0,
  /// `(% (+ i K) N)` is +K, `(% (- i K) N)` is −K (K a constant, N the
  /// binder's range), normalized to (−N/2, N/2] and bounded. Anything else
  /// is not certified.
  std::optional<long long> family_offset(const datum& idx, const binder& b) {
    const datum e = expr_node(idx);  // params fold; the index passes through
    if (e.is_atom()) {
      return e.text() == b.name ? std::optional<long long>(0) : std::nullopt;
    }
    if (e.head() != "%" || e.items().size() != 3) return std::nullopt;
    if (!is_int_atom(e.items()[2]) || int_value(e.items()[2]) != b.hi) {
      return std::nullopt;
    }
    const datum& add = e.items()[1];
    if (add.is_atom() || add.items().size() != 3) return std::nullopt;
    const std::string& op = add.head();
    if (op != "+" && op != "-") return std::nullopt;
    const datum& l = add.items()[1];
    const datum& r = add.items()[2];
    long long k = 0;
    if (l.is_atom() && l.text() == b.name && is_int_atom(r)) {
      k = int_value(r);
    } else if (op == "+" && r.is_atom() && r.text() == b.name &&
               is_int_atom(l)) {
      k = int_value(l);
    } else {
      return std::nullopt;
    }
    if (op == "-") k = -k;
    k %= b.hi;                       // normalize to (−N/2, N/2]
    if (k < 0) k += b.hi;
    if (2 * k > b.hi) k -= b.hi;
    constexpr long long MAX_OFFSET = 4;  // circular sets are nearest-few
    if (k < -MAX_OFFSET || k > MAX_OFFSET) return std::nullopt;
    return k;
  }

  /// Certify one datum of a family body: the index appears only inside a
  /// certified access to a count-N array — rewritten `(at@ ARRAY δ)` — and
  /// params fold. Nullopt on any violation (index as a value, a nested
  /// binder, a grounded or foreign-array access, a 2-D access).
  std::optional<datum> family_node(const datum& d, const binder& b) {
    if (d.is_atom()) {
      if (d.text() == b.name) return std::nullopt;  // index as a value
      const auto it = env_.find(d.text());
      if (it != env_.end()) return int_atom(it->second, d.line());
      return d;
    }
    const std::string& h = d.head();
    if (is_binder_head(h)) return std::nullopt;
    if (h == "at") {
      if (d.items().size() != 3 || !d.items()[1].is_atom()) return std::nullopt;
      const auto ar = arrays_.find(d.items()[1].text());
      if (ar == arrays_.end() || ar->second != b.hi) return std::nullopt;
      const auto delta = family_offset(d.items()[2], b);
      if (!delta) return std::nullopt;
      return list_of("at@", {d.items()[1], int_atom(*delta, d.line())},
                     d.line());
    }
    std::vector<datum> kids;
    for (std::size_t i = 1; i < d.items().size(); ++i) {
      auto k = family_node(d.items()[i], b);
      if (!k) return std::nullopt;
      kids.push_back(std::move(*k));
    }
    return list_of(h, std::move(kids), d.line());
  }

  /// Try `(event NAME (exists (i N) CLAUSE+))` as a certified uniform
  /// family: one binder, full range [0,N), when/do clauses whose every use
  /// of the index is a certified access. On success emits
  /// `(family NAME N CLAUSE'…)` — never enumerated — and returns true; any
  /// failure returns false and the caller expands as usual.
  bool try_family(const datum& d, std::vector<datum>& out) {
    if (!families_ || d.items().size() != 3 || !d.items()[1].is_atom()) {
      return false;
    }
    const datum& ex = d.items()[2];
    if (!ex.is_list() || ex.head() != "exists") return false;
    binder b;
    try {
      b = parse_binder(ex);
    } catch (const expand_error&) {
      return false;
    }
    if (b.lo != 0 || b.hi <= 0) return false;  // C3: the full range
    std::vector<datum> items = {datum::atom("family", d.line()), d.items()[1],
                                int_atom(b.hi, d.line())};
    for (std::size_t c = 2; c < ex.items().size(); ++c) {
      const datum& cl = ex.items()[c];
      if (!cl.is_list() || (cl.head() != "when" && cl.head() != "do")) {
        return false;
      }
      auto n = family_node(cl, b);
      if (!n) return false;
      items.push_back(std::move(*n));
    }
    out.push_back(datum::list(std::move(items), d.line()));
    return true;
  }

  // ---- events and families ---------------------------------------------

  /// Expand the clause list of an event. `forall` splices clauses (the body
  /// is a seq); `exists` forks the whole remainder into one alternative per
  /// index value — an event *family*, one plain event per branch, named by
  /// the accumulated `_v` suffixes.
  branches expand_clauses(const datum* first, const datum* last) {
    if (first == last) return {{"", {}}};
    const datum& c = *first;
    const std::string& h = c.head();
    if (h == "forall") {
      binder b = parse_binder(c);
      bind(b);
      branches acc = {{"", {}}};
      for (long long v = b.lo; v < b.hi; ++v) {
        set(b, v);
        acc = cross(acc, expand_clauses(
                             c.items().data() + 2,
                             c.items().data() + c.items().size()));
      }
      unbind(b);
      return cross(acc, expand_clauses(first + 1, last));
    }
    if (h == "exists") {
      binder b = parse_binder(c);
      branches rest = expand_clauses(first + 1, last);
      branches out;
      bind(b);
      for (long long v = b.lo; v < b.hi; ++v) {
        set(b, v);
        branches sub = expand_clauses(c.items().data() + 2,
                                      c.items().data() + c.items().size());
        for (alt_branch& s : sub)
          out.push_back({"_" + std::to_string(v) + s.suffix,
                         std::move(s.clauses)});
      }
      unbind(b);
      return cross(out, rest);
    }
    datum clause = c;  // (when …) or (do …); anything else translator-reported
    if (h == "when") {
      clause = list_of("when",
                       bexp_list(c.items().data() + 1,
                                 c.items().data() + c.items().size(), true),
                       c.line());
    } else if (h == "do") {
      clause = list_of("do",
                       act_list(c.items().data() + 1,
                                c.items().data() + c.items().size()),
                       c.line());
    }
    branches rest = expand_clauses(first + 1, last);
    return cross({{"", {std::move(clause)}}}, rest);
  }

  void expand_event(const datum& d, std::vector<datum>& out) {
    if (d.items().size() < 2 || !d.items()[1].is_atom())
      throw expand_error(d.line(), "event expects a name");
    const std::string& name = d.items()[1].text();
    branches all = expand_clauses(d.items().data() + 2,
                                  d.items().data() + d.items().size());
    // zero branches (an empty exists range): the event never fires — emit
    // nothing, the honest zero term.
    for (alt_branch& br : all) {
      if (br.clauses.empty())
        throw expand_error(d.line(), "event '" + name +
                                     "' expands to an empty body");
      const std::string full = name + br.suffix;
      if (!br.suffix.empty() && !names_.insert(full).second)
        throw expand_error(d.line(), "expanded event '" + full +
                                     "' collides with a declared name");
      std::vector<datum> items;
      items.push_back(datum::atom("event", d.line()));
      items.push_back(datum::atom(full, d.line()));
      for (datum& cl : br.clauses) items.push_back(std::move(cl));
      out.push_back(datum::list(std::move(items), d.line()));
    }
  }

  // ---- sorts (shape) ---------------------------------------------------

  datum sort_node(const datum& d) {
    if (d.is_atom()) return d;
    const std::string& h = d.head();
    if (h == "pair") {
      if (d.items().size() != 3)
        throw expand_error(d.line(), "pair expects two sorts");
      return list_of("pair",
                     {sort_node(d.items()[1]), sort_node(d.items()[2])},
                     d.line());
    }
    if (h == "spine" || h == "balanced")
      return list_of(h,
                     sort_list(d.items().data() + 1,
                               d.items().data() + d.items().size()),
                     d.line());
    if (h == "at") {
      datum r = at_node(d);
      if (!r.is_atom())
        throw expand_error(d.line(), "the shape needs a grounded cell");
      return r;
    }
    if (is_binder_head(h))
      throw expand_error(d.line(), "a binder in a single sort position: use "
                                   "it inside (spine …) or (balanced …)");
    return d;
  }

  std::vector<datum> sort_list(const datum* first, const datum* last) {
    std::vector<datum> out;
    for (const datum* it = first; it != last; ++it) {
      const std::string& h = it->head();
      if (h == "exists")
        throw expand_error(it->line(), "exists has no meaning in a shape");
      if (h == "forall") {
        binder b = parse_binder(*it);
        bind(b);
        for (long long v = b.lo; v < b.hi; ++v) {
          set(b, v);
          std::vector<datum> inst =
              sort_list(it->items().data() + 2,
                        it->items().data() + it->items().size());
          for (datum& s : inst) out.push_back(std::move(s));
        }
        unbind(b);
        continue;
      }
      out.push_back(sort_node(*it));
    }
    return out;
  }

  // ---- pair lists (init / word) ---------------------------------------

  std::vector<datum> pair_list(const datum* first, const datum* last) {
    std::vector<datum> out;
    for (const datum* it = first; it != last; ++it) {
      const std::string& h = it->head();
      if (h == "exists")
        throw expand_error(it->line(), "exists has no meaning in a binding "
                                       "list");
      if (h == "forall") {
        binder b = parse_binder(*it);
        bind(b);
        for (long long v = b.lo; v < b.hi; ++v) {
          set(b, v);
          std::vector<datum> inst =
              pair_list(it->items().data() + 2,
                        it->items().data() + it->items().size());
          for (datum& p : inst) out.push_back(std::move(p));
        }
        unbind(b);
        continue;
      }
      if (!it->is_list() || it->items().size() != 2)
        throw expand_error(it->line(), "expected a (NAME VALUE) pair");
      out.push_back(datum::list(
          {expr_node(it->items()[0]), expr_node(it->items()[1])}, it->line()));
    }
    return out;
  }

  /// Sniff whether an `init` binder body starts with pairs or an EVTERM:
  /// a pair is a 2-list whose first item is a plain (or grounded-`at`) name.
  bool starts_with_pair(const datum& bd) {
    if (bd.items().size() < 3) return false;
    const datum& f = bd.items()[2];
    if (!f.is_list() || f.items().size() != 2) return false;
    const datum& lhs = f.items()[0];
    if (lhs.is_atom())
      return !is_evterm_head(lhs.text()) && !is_binder_head(lhs.text());
    return lhs.head() == "at";
  }

  // ---- top level -------------------------------------------------------

  void top_forms(const std::vector<datum>& forms, std::vector<datum>& out) {
    for (const datum& f : forms) top_form(f, out);
  }

  void top_form(const datum& f, std::vector<datum>& out) {
    const std::string& h = f.head();
    const datum* body = f.items().data() + 1;
    const datum* end = f.items().data() + f.items().size();
    if (h == "param") {
      if (f.items().size() != 3 || !f.items()[1].is_atom() ||
          is_int_atom(f.items()[1]))
        throw expand_error(f.line(), "param expects (param NAME EXPR)");
      const std::string& name = f.items()[1].text();
      if (env_.count(name))
        throw expand_error(f.line(), "param '" + name + "' redefined");
      if (names_.count(name))
        throw expand_error(f.line(), "param '" + name + "' collides with a "
                                     "declared name");
      env_[name] = eval_int(f.items()[2]);
      return;  // the form is consumed
    }
    if (h == "array" && f.items().size() >= 3) {
      datum count = expr_node(f.items()[2]);
      if (is_int_atom(count)) {
        lower_count_array(f, int_value(count), out);
        return;
      }
      out.push_back(f);  // explicit-cell form, untouched
      return;
    }
    if (h == "leaf") {
      std::vector<datum> kids;
      for (const datum* it = body; it != end; ++it)
        kids.push_back(expr_node(*it));
      out.push_back(list_of("leaf", std::move(kids), f.line()));
      return;
    }
    if (h == "shape") {
      if (f.items().size() != 2)
        throw expand_error(f.line(), "shape expects one sort");
      out.push_back(list_of("shape", {sort_node(f.items()[1])}, f.line()));
      return;
    }
    if (h == "event") {
      if (!try_family(f, out)) expand_event(f, out);
      return;
    }
    if (h == "alt" || h == "seq") {
      if (f.items().size() < 3 || !f.items()[1].is_atom())
        throw expand_error(f.line(), h + " expects a name then terms");
      std::vector<datum> terms =
          evterm_list(body + 1, end, /*seq=*/h == "seq");
      std::vector<datum> items = {f.items()[0], f.items()[1]};
      for (datum& t : terms) items.push_back(std::move(t));
      out.push_back(datum::list(std::move(items), f.line()));
      return;
    }
    if (h == "init" || h == "word") {
      if (h == "word" && (f.items().size() < 2 || !f.items()[1].is_atom()))
        throw expand_error(f.line(), "word expects a name");
      std::vector<datum> items(f.items().begin(),
                               f.items().begin() + (h == "word" ? 2 : 1));
      const datum* rest = body + (h == "word" ? 1 : 0);
      if (rest != end && rest->is_list() &&
          (is_evterm_head(rest->head()) ||
           (is_binder_head(rest->head()) && !starts_with_pair(*rest)))) {
        for (const datum* it = rest; it != end; ++it)
          items.push_back(evterm_node(*it));
      } else {
        for (datum& p : pair_list(rest, end)) items.push_back(std::move(p));
      }
      out.push_back(datum::list(std::move(items), f.line()));
      return;
    }
    if (h == "reach" || h == "apply") {
      std::vector<datum> items = {f.items()[0]};
      for (const datum* it = body; it != end; ++it)
        items.push_back(it->is_list() ? evterm_node(*it) : *it);
      out.push_back(datum::list(std::move(items), f.line()));
      return;
    }
    if (h == "select") {
      if (f.items().size() < 4)
        throw expand_error(f.line(), "select expects NAME SOURCE QATOM+");
      std::vector<datum> items = {f.items()[0], f.items()[1], f.items()[2]};
      for (datum& q : bexp_list(body + 2, end, /*conj=*/true))
        items.push_back(std::move(q));
      out.push_back(datum::list(std::move(items), f.line()));
      return;
    }
    if (h == "forall") {
      binder b = parse_binder(f);
      bind(b);
      for (long long v = b.lo; v < b.hi; ++v) {
        set(b, v);
        top_forms({f.items().begin() + 2, f.items().end()}, out);
      }
      unbind(b);
      return;
    }
    if (h == "exists")
      throw expand_error(f.line(), "exists has no meaning at top level");
    // consumers (count, nodes, expect, states, …): substitute and fold
    if (f.is_list() && !f.items().empty()) {
      std::vector<datum> items = {f.items()[0]};
      for (const datum* it = body; it != end; ++it)
        items.push_back(expr_node(*it));
      out.push_back(datum::list(std::move(items), f.line()));
      return;
    }
    out.push_back(f);
  }

  /// Lower a count-form array. Arity decides the form: 3 is 1-D open,
  /// 5 is 1-D bounded, 4 is 2-D open, 6 is 2-D bounded. A 2-D array's
  /// cells are NAME_i_j in row-major order, grouped by one flat explicit
  /// (array …) so an open (linearized) index stays an ordinary access.
  void lower_count_array(const datum& f, long long count,
                         std::vector<datum>& out) {
    const std::size_t n = f.items().size();
    if (n < 3 || n > 6)
      throw expand_error(f.line(), "count-form array expects "
                                   "(array NAME COUNT [COLS] [LO HI])");
    if (count < 0) throw expand_error(f.line(), "negative array size");
    const std::string& name = f.items()[1].text();
    const bool two_d = (n == 4 || n == 6);
    long long cols = 1;
    if (two_d) {
      datum c = expr_node(f.items()[3]);
      if (!is_int_atom(c))
        throw expand_error(f.line(), "2-D array's second dimension does not "
                                     "fold to an integer");
      cols = int_value(c);
      if (cols < 0) throw expand_error(f.line(), "negative array size");
    }
    std::vector<datum> bounds;
    if (n >= 5) {
      bounds.push_back(expr_node(f.items()[n - 2]));
      bounds.push_back(expr_node(f.items()[n - 1]));
      if (!is_int_atom(bounds[0]) || !is_int_atom(bounds[1]))
        throw expand_error(f.line(), "array bounds do not fold to integers");
    }
    std::vector<datum> cells = {datum::atom("array", f.line()),
                                datum::atom(name, f.line())};
    const long long total = two_d ? count * cols : count;
    for (long long k = 0; k < total; ++k) {
      const std::string cell =
          two_d ? name + "_" + std::to_string(k / cols) + "_" +
                      std::to_string(k % cols)
                : name + "_" + std::to_string(k);
      if (!names_.insert(cell).second)
        throw expand_error(f.line(), "generated cell '" + cell +
                                     "' collides with a declared name");
      std::vector<datum> leaf = {datum::atom("leaf", f.line()),
                                 datum::atom(cell, f.line())};
      for (const datum& bd : bounds) leaf.push_back(bd);
      out.push_back(datum::list(std::move(leaf), f.line()));
      cells.push_back(datum::atom(cell, f.line()));
    }
    out.push_back(datum::list(std::move(cells), f.line()));
    if (two_d)
      arrays2_[name] = {count, cols};
    else
      arrays_[name] = count;
  }
};

}  // namespace

std::vector<datum> expand(const std::vector<datum>& forms, bool families) {
  return expander{families}.run(forms);
}

}  // namespace hsc::surface
