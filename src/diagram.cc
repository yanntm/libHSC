/// \file diagram.cc
/// \brief Construction 3.3 and the set algebra over it.
///
/// Re-expressed from libDDD's SDED (`ddd/SDED.cpp`: `_SDED_Add::eval`,
/// `_SDED_Mult::eval`, `_SDED_Minus::eval`, `square_union`), which is the
/// same algorithm written for one sort; libsdd's `dd/square_union.hh` is the
/// same accumulator again. Code is new, the legacy source is the spec.

#include "hsc/core/diagram.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <ostream>
#include <unordered_set>

#include "hsc/core/manager.hh"

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
/// Linear search rather than a map: node arities are small, and the caller's
/// meet loop is quadratic anyway.
class diagram_engine::accumulator {
 public:
  explicit accumulator(support_algebra& head) : head_(head) {}

  void add(code sub, code prime) {
    if (sub == none || prime == none) return;  // smash, before anything
    for (arc& a : entries_) {
      if (a.sub == sub) {
        a.prime = a.prime == none ? prime : head_.join(a.prime, prime);
        return;
      }
    }
    entries_.push_back({prime, sub});
  }

  [[nodiscard]] std::vector<arc>& entries() noexcept { return entries_; }

 private:
  support_algebra& head_;
  std::vector<arc> entries_;
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
  accumulator acc(head_algebra(sort));
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

code diagram_engine::do_join(code a, code b) {
  const shape_code sort = nodes_[a].sort;
  assert(nodes_[b].sort == sort && "join across different sorts");

  accumulator acc(head_algebra(sort));
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
  accumulator acc(head);
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
  accumulator acc(head);
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

double diagram_engine::cardinal(code c) const {
  if (c == none) return 0.0;
  const node& n = nodes_[c];
  if (n.arity == 0) return 1.0;  // the terminal: one word, the empty one

  if (cardinal_memo_.size() <= c) cardinal_memo_.resize(c + 1, -1.0);
  if (cardinal_memo_[c] >= 0.0) return cardinal_memo_[c];

  const support_algebra& head = head_algebra(n.sort);
  const support_algebra& tail = tail_algebra(n.sort);
  double total = 0.0;
  for (const arc& a : n.arcs()) {
    total += head.cardinal(a.prime) * tail.cardinal(a.sub);
  }
  if (cardinal_memo_.size() <= c) cardinal_memo_.resize(c + 1, -1.0);
  cardinal_memo_[c] = total;
  return total;
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
