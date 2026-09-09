/// \file diagram.cc
/// \brief The canonicalizer and the set algebra over it.
///
/// Re-expressed from libDDD's SDED (`ddd/SDED.cpp`: `_SDED_Add::eval`,
/// `_SDED_Mult::eval`, `_SDED_Minus::eval`, `square_union`), which is the
/// same algorithm written for one sort; libsdd's `dd/square_union.hh` is the
/// same accumulator again. Code is new, the legacy source is the spec.

#include "hsc/core/diagram.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <ostream>
#include <unordered_set>

#include "hsc/core/manager.hh"
#include "hsc/core/operation.hh"
#include "hsc/util/hash.hh"

namespace hsc::core {

namespace {

/// The probe view for a node: a node that has not been built.
struct node_view {
  shape_code sort;
  std::span<const arc> arcs;

  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_value(sort);
    for (const arc& a : arcs) {
      util::hash_combine(seed, a.prime);
      util::hash_combine(seed, a.sub);
    }
    return seed;
  }
  [[nodiscard]] bool equals(const node& n) const {
    return n.sort == sort && std::ranges::equal(n.arcs(), arcs);
  }
  [[nodiscard]] std::size_t extra_bytes() const {
    return arcs.size() * sizeof(arc);
  }
  node* construct(void* mem) const {
    auto* p = new (mem) node{sort, static_cast<std::uint32_t>(arcs.size())};
    std::copy(arcs.begin(), arcs.end(), const_cast<arc*>(p->data()));
    return p;
  }
};

}  // namespace

/// \brief The accumulator: arcs indexed by sub.
///
/// libDDD's `square_union`, and libsdd's class of the same name. Keying by
/// sub is what makes (F) — distinct subs — an invariant of construction
/// rather than a pass at the end: adding an arc whose sub is already present
/// joins the two primes instead of appending a second arc.
///
/// A linear search while the node is small; past a few entries, an
/// open-addressing index from sub to entry position — a wide head (an
/// integer domain of a hundred values) makes the regroup quadratic
/// otherwise. Entries are only ever appended and their subs never change,
/// so the index stays valid while the sieve rewrites primes.
class diagram_engine::accumulator {
 public:
  /// \p expected is a capacity hint — the arcs of the operands — so the
  /// entries do not regrow arc by arc.
  accumulator(support_algebra& head, std::size_t expected) : head_(head) {
    entries_.reserve(expected);
  }

  void add(code sub, code prime) {
    if (sub == none || prime == none) return;  // smash, before anything
    if (index_.empty()) {
      for (arc& a : entries_) {
        if (a.sub == sub) {
          merge(a, prime);
          return;
        }
      }
      entries_.push_back({prime, sub});
      if (entries_.size() > kLinear) reindex(4 * kLinear);
      return;
    }
    std::size_t slot = probe(sub);
    if (index_[slot] != npos) {
      merge(entries_[index_[slot]], prime);
      return;
    }
    index_[slot] = static_cast<std::uint32_t>(entries_.size());
    entries_.push_back({prime, sub});
    if (2 * entries_.size() > index_.size()) reindex(2 * index_.size());
  }

  [[nodiscard]] std::vector<arc>& entries() noexcept { return entries_; }

 private:
  static constexpr std::size_t kLinear = 8;
  static constexpr std::uint32_t npos = ~std::uint32_t{0};

  void merge(arc& a, code prime) {
    a.prime = a.prime == none ? prime : head_.join(a.prime, prime);
  }
  /// The slot holding \p sub, or the empty slot where it would go.
  [[nodiscard]] std::size_t probe(code sub) const noexcept {
    const std::size_t mask = index_.size() - 1;
    std::size_t i = util::mix32(sub) & mask;
    while (index_[i] != npos && entries_[index_[i]].sub != sub) i = (i + 1) & mask;
    return i;
  }
  void reindex(std::size_t size) {
    index_.assign(size, npos);  // size is a power of two
    for (std::size_t k = 0; k < entries_.size(); ++k) {
      index_[probe(entries_[k].sub)] = static_cast<std::uint32_t>(k);
    }
  }

  support_algebra& head_;
  std::vector<arc> entries_;
  std::vector<std::uint32_t> index_;  ///< empty while the search is linear
};

diagram_engine::diagram_engine(manager& owner, std::size_t cache_capacity)
    : owner_(owner), ops_(*this, cache_capacity) {
  one_ = nodes_.get(node_view{owner_.shapes().unit(), {}});
}

support_algebra& diagram_engine::head_algebra(shape_code sort) const {
  return owner_.algebra(owner_.shapes().head(sort));
}

/// The algebra the subs of a node of sort \p sort live in. Dispatched just
/// like the head: at sort `(<A>, <B>)` the subs are theory codes, at
/// `(V, (W, X))` they are nodes, at `(V, 1)` they are the terminal.
support_algebra& diagram_engine::tail_algebra(shape_code sort) const {
  return owner_.algebra(owner_.shapes().tail(sort));
}

code diagram_engine::intern_node(shape_code sort, std::span<const arc> arcs) {
  if (arcs.empty()) return none;
  return nodes_.get(node_view{sort, arcs});
}

/// Drop dead entries, order canonically, intern.
code diagram_engine::finish(shape_code sort, accumulator& acc) {
  auto& entries = acc.entries();
  std::erase_if(entries, [](const arc& a) {
    return a.prime == none || a.sub == none;
  });
  std::ranges::sort(entries, {}, &arc::prime);
  return intern_node(sort, entries);
}

/// \brief Sieve one operand into the accumulator, keeping primes disjoint.
///
/// \p operand's own primes must be pairwise disjoint (they are: an operand is
/// either a canonical node or a single rectangle). For each incoming arc, the
/// entries already present are carved by relative difference until the arc is
/// used up. Results are buffered and only merged afterwards, because an entry
/// created by this arc must not be re-sieved against it.
void diagram_engine::sieve(shape_code sort, accumulator& acc,
                           std::span<const arc> operand) {
  support_algebra& head = head_algebra(sort);
  support_algebra& tail = tail_algebra(sort);
  std::vector<arc>& entries = acc.entries();
  std::vector<arc> merged;  // overlaps, and leftovers of the incoming arcs

  for (const arc& incoming : operand) {
    if (incoming.prime == none || incoming.sub == none) continue;
    code rest = incoming.prime;

    for (arc& e : entries) {
      if (e.prime == none) continue;  // consumed by an earlier arc
      if (rest == none) break;

      // Equality first: it is a pointer test, and meet is work.
      if (e.prime == rest) {
        merged.push_back({rest, tail.join(e.sub, incoming.sub)});
        e.prime = none;
        rest = none;
        break;
      }

      const code overlap = head.meet(e.prime, rest);
      if (overlap == none) continue;

      merged.push_back({overlap, tail.join(e.sub, incoming.sub)});
      if (overlap == e.prime) {
        e.prime = none;  // the entry is entirely inside the incoming arc
      } else {
        e.prime = head.minus(e.prime, overlap);
      }
      rest = head.minus(rest, overlap);
    }

    if (rest != none) merged.push_back({rest, incoming.sub});
  }

  for (const arc& a : merged) acc.add(a.sub, a.prime);
}

code diagram_engine::canonize(shape_code sort, std::span<const arc> rectangles) {
  accumulator acc(head_algebra(sort), rectangles.size());
  // Each rectangle is its own operand: nothing is assumed about the bag.
  for (const arc& r : rectangles) sieve(sort, acc, std::span(&r, 1));
  return finish(sort, acc);
}

code diagram_engine::rectangle(shape_code sort, code prime, code sub) {
  if (prime == none || sub == none) return none;
  const arc a{prime, sub};
  return intern_node(sort, std::span(&a, 1));
}

// --- the set algebra ------------------------------------------------------

code binary_op::operator()(diagram_engine& engine) const {
  switch (op) {
    case kind::join:
      return engine.do_join(a, b);
    case kind::meet:
      return engine.do_meet(a, b);
    case kind::minus:
      return engine.do_minus(a, b);
    case kind::apply:
      return engine.do_apply(a, b);
    case kind::has_image:
      return engine.do_has_image(a, b);
  }
  return none;
}

code diagram_engine::join(code a, code b) {
  if (a == none) return b;
  if (b == none) return a;
  if (a == b) return a;
  return ops_(binary_op(binary_op::kind::join, a, b));
}

code diagram_engine::meet(code a, code b) {
  if (a == none || b == none) return none;
  if (a == b) return a;
  return ops_(binary_op(binary_op::kind::meet, a, b));
}

code diagram_engine::minus(code a, code b) {
  if (a == none || b == none) return a;
  if (a == b) return none;
  return ops_(binary_op(binary_op::kind::minus, a, b));
}

code diagram_engine::apply_local(code term, code value) {
  if (term == op_table::id) return value;  // id is free
  if (value == none) return none;
  return ops_(binary_op(binary_op::kind::apply, term, value));
}

code diagram_engine::has_image(code term, code value) {
  if (term == op_table::id) return value;
  if (value == none) return none;
  return ops_(binary_op(binary_op::kind::has_image, term, value));
}

bool diagram_engine::cache_results() const noexcept { return !owner_.partial(); }

code diagram_engine::gfp_at(shape_code sort, code term, code value) {
  if (value == none || term == op_table::id) return value;
  if (owner_.shapes().kind(sort) == shape_kind::pair) {
    return apply_local(owner_.operations().gfp(term), value);
  }
  support_algebra& algebra = owner_.algebra(sort);
  code x = value;
  for (;;) {
    if (owner_.stopping()) {  // a round boundary: what we have, marked partial
      owner_.mark_partial();
      return x;
    }
    ++gfp_rounds_;
    const code y = algebra.meet(x, algebra.apply_local(term, x));
    if (y == x || y == none) return y;
    x = y;
  }
}

code diagram_engine::gfp_witness_at(shape_code sort, code term, code value) {
  if (owner_.shapes().kind(sort) == shape_kind::pair) {
    return has_image(owner_.operations().gfp(term), value);
  }
  return gfp_at(sort, term, value);  // a leaf: small, in full
}

/// \brief The existential image of one term against one diagram
/// (`algorithm.md` §10): a nonempty subset of the image, or `none` iff the
/// image is empty. The full image is the fallback wherever a witness is not
/// cheaper.
code diagram_engine::do_has_image(code term, code d) {
  op_table& ops = owner_.operations();
  const op_term& t = ops[term];
  switch (t.kind) {
    case op_kind::sum:
      for (const code s : t.operands()) {
        const code w = has_image(s, d);
        if (w != none) return w;
      }
      return none;
    case op_kind::compose: {
      const code wb = has_image(t.operand(1), d);
      if (wb == none) return none;
      const code wa = has_image(t.operand(0), wb);
      if (wa != none) return wa;
      return has_image(t.operand(0), apply_local(t.operand(1), d));
    }
    case op_kind::lfp:
    case op_kind::saturate:
      return d;  // a closure contains its seed
    case op_kind::within:
      return meet(d, t.operand(0));
    case op_kind::expr:
      return do_apply(term, d);  // the case engine, in full
    case op_kind::gfp: {
      // A cycle of a part of the events inside a part of the set is a cycle
      // of the whole: try below the cut and on the head before the full hull
      // — when asked to; the descent is a loss where no component cycles.
      if (!fast_cycle_witness_) return do_apply(term, d);
      const node& n = nodes_[d];
      const shape_code sort = n.sort;
      const shape_code hs = owner_.shapes().head(sort);
      const shape_code ts = owner_.shapes().tail(sort);
      std::vector<code> flat;
      {
        std::vector<code> stack{t.operand(0)};
        while (!stack.empty()) {
          const code e = stack.back();
          stack.pop_back();
          if (e != op_table::id && ops[e].kind == op_kind::sum) {
            for (const code c : ops[e].operands()) stack.push_back(c);
          } else {
            flat.push_back(e);
          }
        }
      }
      std::vector<code> below, edge;
      for (const code e : flat) {
        if (e == op_table::id || ops[e].kind != op_kind::node) continue;
        if (ops[e].operand(0) == op_table::id) below.push_back(ops[e].operand(1));
        else if (ops[e].operand(1) == op_table::id) edge.push_back(ops[e].operand(0));
      }
      const code f_tail = below.empty() ? op_table::id : sum_at(owner_, ts, below);
      const code l_head = edge.empty() ? op_table::id : sum_at(owner_, hs, edge);
      for (const arc& x : n.arcs()) {
        if (f_tail != op_table::id) {
          const code w = gfp_witness_at(ts, f_tail, x.sub);
          if (w != none) return rectangle(sort, x.prime, w);
        }
        if (l_head != op_table::id) {
          const code w = gfp_witness_at(hs, l_head, x.prime);
          if (w != none) return rectangle(sort, w, x.sub);
        }
      }
      return do_apply(term, d);  // the full hull
    }
    case op_kind::node: {
      const node& n = nodes_[d];
      assert(n.arity != 0 && "operation term reaches past its sort");
      const shape_code sort = n.sort;
      support_algebra& head = head_algebra(sort);
      support_algebra& tail = tail_algebra(sort);
      for (const arc& x : n.arcs()) {
        const code wh = t.operand(0) == op_table::id
                            ? x.prime
                            : head.has_image_local(t.operand(0), x.prime);
        if (wh == none) continue;
        const code wt = t.operand(1) == op_table::id
                            ? x.sub
                            : tail.has_image_local(t.operand(1), x.sub);
        if (wt == none) continue;
        return rectangle(sort, wh, wt);
      }
      return none;
    }
  }
  return none;
}

/// \brief Evaluate one operation term against one diagram.
///
/// The whole local fragment of the term algebra. `node(h,t)` maps every arc through the
/// head and tail algebras and re-canonicalises; which algebra interprets `h`
/// is settled by the shape, so a leaf theory and a nested diagram are the
/// same case (Cor. 3.6).
code diagram_engine::term_sum(code a, code b) {
  return owner_.operations().sum(a, b);
}

code diagram_engine::invert_local(code term, code domain) {
  if (domain == none) return owner_.operations().within(none);
  return inverter(owner_)(sort_of(domain), term, domain);
}

code diagram_engine::term_lfp(code t) {
  // The naive lfp. The saturating one needs the sort, so it is
  // core::saturate() in operation.hh.
  return owner_.operations().lfp(t);
}

code diagram_engine::do_apply(code term, code d) {
  const op_term& t = owner_.operations()[term];
  switch (t.kind) {
    case op_kind::sum: {
      code result = none;
      for (const code s : t.operands()) result = join(result, apply_local(s, d));
      return result;
    }
    case op_kind::compose:
      return apply_local(t.operand(0), apply_local(t.operand(1), d));

    case op_kind::lfp: {
      // Round-based iteration, kept for comparison: it rebuilds a fresh
      // object every round and so misses the memo every round.
      const code h = t.operand(0);
      code x = d;
      for (;;) {
        if (owner_.stopping()) {
          owner_.mark_partial();
          return x;
        }
        const code y = join(x, apply_local(h, x));
        if (y == x) return x;
        x = y;
      }
    }

    case op_kind::within:
      // The constant selector: keep what lies in the diagram.
      return meet(d, t.operand(0));

    case op_kind::gfp: {
      // The deflationary closure: shrink from the argument until nothing
      // leaves. Every round is a subset of the previous, so it halts.
      const code h = t.operand(0);
      code x = d;
      for (;;) {
        if (owner_.stopping()) {
          owner_.mark_partial();
          return x;
        }
        ++gfp_rounds_;
        const code y = meet(x, apply_local(h, x));
        if (y == x) return x;
        x = y;
      }
    }

    case op_kind::saturate: {
      // The F-L-G schedule, as libsdd's _saturation_fixpoint::operator()
      // and libDDD's Fixpoint::eval both run it: settle everything below,
      // then the edge, then chain the crossing events, until nothing moves.
      // F and L are already closures, built by the rewrite; the recursion
      // that makes this hierarchical happened there, not here.
      const std::span<const code> parts = t.operands();
      const code f_part = parts[0];
      const code l_part = parts[1];
      code current = d;
      code previous = none;
      do {
        if (owner_.stopping()) {
          owner_.mark_partial();
          return current;
        }
        previous = current;
        current = apply_local(f_part, current);
        current = apply_local(l_part, current);
        for (const code g : parts.subspan(2)) {
          current = join(current, apply_local(g, current));
        }
      } while (current != previous);
      return current;
    }

    case op_kind::expr:
      // A case bracket: opaque to core, the registered engine's job.
      assert(owner_.cases() != nullptr && "expr term with no case engine");
      return owner_.cases()->apply(term, d);

    case op_kind::node:
      break;
  }

  const node& n = nodes_[d];
  assert(n.arity != 0 && "operation term reaches past its sort");
  const shape_code sort = n.sort;
  support_algebra& head = head_algebra(sort);
  support_algebra& tail = tail_algebra(sort);

  if (t.operand(0) == op_table::id) {
    // Skip on the head: the primes are untouched, so they are still pairwise
    // disjoint and only the regroup by sub is owed. The sieve never runs.
    accumulator acc(head, n.arity);
    for (const arc& x : n.arcs()) {
      acc.add(tail.apply_local(t.operand(1), x.sub), x.prime);
    }
    return finish(sort, acc);
  }

  if (head.injective(t.operand(0))) {
    // The head acts injectively: the image primes stay pairwise disjoint,
    // so again only the regroup by sub is owed.
    accumulator acc(head, n.arity);
    for (const arc& x : n.arcs()) {
      const code prime = head.apply_local(t.operand(0), x.prime);
      if (prime == none) continue;
      acc.add(tail.apply_local(t.operand(1), x.sub), prime);
    }
    return finish(sort, acc);
  }

  // The head acts, so primes may now overlap or collide: full construction.
  std::vector<arc> bag;
  bag.reserve(n.arity);
  for (const arc& x : n.arcs()) {
    const code prime = head.apply_local(t.operand(0), x.prime);
    if (prime == none) continue;
    const code sub = tail.apply_local(t.operand(1), x.sub);
    if (sub == none) continue;
    bag.push_back({prime, sub});
  }
  return canonize(sort, bag);
}

code diagram_engine::do_join(code a, code b) {
  const shape_code sort = nodes_[a].sort;
  assert(nodes_[b].sort == sort && "join across different sorts");

  accumulator acc(head_algebra(sort), nodes_[a].arity + nodes_[b].arity);
  for (const arc& x : nodes_[a].arcs()) acc.add(x.sub, x.prime);
  sieve(sort, acc, nodes_[b].arcs());
  return finish(sort, acc);
}

code diagram_engine::do_meet(code a, code b) {
  const shape_code sort = nodes_[a].sort;
  assert(nodes_[b].sort == sort && "meet across different sorts");
  support_algebra& head = head_algebra(sort);
  support_algebra& tail = tail_algebra(sort);

  // The primes produced here are pairwise disjoint already — both operands
  // are partitions — so the sieve is not needed, only the grouping by sub.
  accumulator acc(head, nodes_[a].arity + nodes_[b].arity);
  for (const arc& x : nodes_[a].arcs()) {
    for (const arc& y : nodes_[b].arcs()) {
      const code overlap = head.meet(x.prime, y.prime);
      if (overlap == none) continue;
      acc.add(tail.meet(x.sub, y.sub), overlap);
      if (overlap == x.prime) break;  // x is used up
    }
  }
  return finish(sort, acc);
}

code diagram_engine::do_minus(code a, code b) {
  const shape_code sort = nodes_[a].sort;
  assert(nodes_[b].sort == sort && "difference across different sorts");
  support_algebra& head = head_algebra(sort);
  support_algebra& tail = tail_algebra(sort);

  // Also disjoint by construction: each prime of a is partitioned among the
  // primes of b, plus the part of it that b does not cover at all.
  accumulator acc(head, nodes_[a].arity + nodes_[b].arity);
  for (const arc& x : nodes_[a].arcs()) {
    code rest = x.prime;
    for (const arc& y : nodes_[b].arcs()) {
      const code overlap = head.meet(x.prime, y.prime);
      if (overlap == none) continue;
      acc.add(tail.minus(x.sub, y.sub), overlap);
      rest = head.minus(rest, overlap);
      if (rest == none) break;
    }
    if (rest != none) acc.add(x.sub, rest);
  }
  return finish(sort, acc);
}

// --- reading a diagram ----------------------------------------------------

template <class Num, class Lookup, class Store>
Num diagram_engine::cardinal_as(code c, Lookup&& lookup, Store&& store) const {
  if (c == none) return Num(0);
  const node& n = nodes_[c];
  if (n.arity == 0) return Num(1);  // the terminal: one word, the empty one
  if (const Num* hit = lookup(c)) return *hit;

  const shape_table& shapes = owner_.shapes();
  const bool head_is_diagram = !shapes.is_leaf(shapes.head(n.sort));
  const bool tail_is_diagram = !shapes.is_leaf(shapes.tail(n.sort));
  const support_algebra& head = head_algebra(n.sort);
  const support_algebra& tail = tail_algebra(n.sort);
  // A leaf theory answers in double; its sets are small enough (below 2^53)
  // for the conversion to be exact. A diagram operand recurses in Num.
  auto side = [&](const support_algebra& alg, bool is_diagram, code x) {
    return is_diagram ? cardinal_as<Num>(x, lookup, store)
                      : Num(alg.cardinal(x));
  };
  Num total(0);
  for (const arc& a : n.arcs()) {
    total += side(head, head_is_diagram, a.prime) *
             side(tail, tail_is_diagram, a.sub);
  }
  store(c, total);
  return total;
}

double diagram_engine::cardinal(code c) const {
  return cardinal_as<double>(
      c,
      [&](code x) -> const double* {
        if (cardinal_memo_.size() <= x || cardinal_memo_[x] < 0.0) return nullptr;
        return &cardinal_memo_[x];
      },
      [&](code x, double v) {
        if (cardinal_memo_.size() <= x) cardinal_memo_.resize(x + 1, -1.0);
        cardinal_memo_[x] = v;
      });
}

mpz_class diagram_engine::cardinal_exact(code c) const {
  return cardinal_as<mpz_class>(
      c,
      [&](code x) -> const mpz_class* {
        const auto it = exact_memo_.find(x);
        return it == exact_memo_.end() ? nullptr : &it->second;
      },
      [&](code x, const mpz_class& v) { exact_memo_.emplace(x, v); });
}

void diagram_engine::collect_nodes(code c, std::unordered_set<code>& seen) const {
  if (c == none) return;
  if (!seen.insert(c).second) return;
  const node& n = nodes_[c];
  if (n.arity == 0) return;  // the terminal: sort `1` has no head or tail
  const shape_table& shapes = owner_.shapes();
  const bool head_is_diagram = !shapes.is_leaf(shapes.head(n.sort));
  const bool tail_is_diagram = !shapes.is_leaf(shapes.tail(n.sort));
  for (const arc& a : n.arcs()) {
    if (head_is_diagram) collect_nodes(a.prime, seen);
    if (tail_is_diagram) collect_nodes(a.sub, seen);
  }
}

std::size_t diagram_engine::size(code c) const {
  std::unordered_set<code> seen;
  collect_nodes(c, seen);
  return seen.size();
}

void diagram_engine::print(std::ostream& os, code c) const {
  if (c == none) {
    os << "0";
    return;
  }
  const node& n = nodes_[c];
  if (n.arity == 0) {
    os << "1";
    return;
  }
  const support_algebra& head = head_algebra(n.sort);
  const support_algebra& tail = tail_algebra(n.sort);
  os << '[';
  bool first = true;
  for (const arc& a : n.arcs()) {
    if (!first) os << " + ";
    first = false;
    head.print(os, a.prime);
    os << "->";
    tail.print(os, a.sub);
  }
  os << ']';
}

}  // namespace hsc::core
