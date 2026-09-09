/// \file operation.cc
/// \brief Term construction, event assembly, and the saturation rewrite.
///
/// The rewrite is re-expressed from libsdd's `sdd/hom/rewrite.hh` (the static
/// pass) and libDDD's `Add::skip_variable` partition cache in `ddd/SHom.cpp`;
/// the evaluation schedule it produces is in `diagram.cc`. libDDD's
/// `demo/hanoi/hanoiHom.cpp` is the same schedule hand-written, before it had
/// a name.

#include "hsc/core/operation.hh"

#include "hsc/core/diagram.hh"
#include "hsc/util/errors.hh"

#include <algorithm>
#include <cstdint>
#include <new>

#include "hsc/core/manager.hh"

namespace hsc::core {

namespace {

/// The probe view for a term: a term that has not been built.
struct op_view {
  op_kind kind;
  std::span<const code> operands;

  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_value(static_cast<std::uint8_t>(kind));
    util::hash_range(seed, operands.begin(), operands.end());
    return seed;
  }
  [[nodiscard]] bool equals(const op_term& t) const {
    return t.kind == kind && std::ranges::equal(t.operands(), operands);
  }
  [[nodiscard]] std::size_t extra_bytes() const {
    return operands.size() * sizeof(code);
  }
  op_term* construct(void* mem) const {
    auto* p = new (mem)
        op_term{kind, static_cast<std::uint32_t>(operands.size())};
    std::copy(operands.begin(), operands.end(), const_cast<code*>(p->data()));
    return p;
  }
};

}  // namespace

code op_table::make(op_kind kind, std::span<const code> operands) {
  return table_.get(op_view{kind, operands});
}

// --- assembling an event --------------------------------------------------

namespace {

code product_at(op_table& ops, const shape_table& shapes, shape_code sort,
                std::span<const code> by_leaf, std::size_t& next) {
  switch (shapes.kind(sort)) {
    case shape_kind::unit:
      return op_table::id;  // no frontier: nothing addresses it
    case shape_kind::leaf:
      return next < by_leaf.size() ? by_leaf[next++] : op_table::id;
    case shape_kind::pair: {
      // Head before tail: the frontier is read left to right.
      const code head =
          product_at(ops, shapes, shapes.head(sort), by_leaf, next);
      const code tail =
          product_at(ops, shapes, shapes.tail(sort), by_leaf, next);
      return ops.node(head, tail);
    }
  }
  return op_table::id;
}

}  // namespace

code product(op_table& ops, const shape_table& shapes, shape_code sort,
             std::span<const code> by_leaf) {
  std::size_t next = 0;
  return product_at(ops, shapes, sort, by_leaf, next);
}

// --- the saturation rewrite ------------------------------------------

namespace {

/// Inline the operands of every `sum` in \p e into \p out — the summand
/// list a folded (or named) sum denotes. Only meaningful at a composite
/// sort, where codes are op-table terms.
void flatten_into(const op_table& ops, code e, std::vector<code>& out) {
  if (e != op_table::id && ops[e].kind == op_kind::sum) {
    for (const code c : ops[e].operands()) flatten_into(ops, c, out);
    return;
  }
  out.push_back(e);
}

}  // namespace

code sum_at(manager& mgr, shape_code sort, std::span<const code> events) {
  const shape_table& shapes = mgr.shapes();
  op_table& ops = mgr.operations();

  // A leaf sort: the codes are theory terms and the theory owns their sum.
  if (shapes.kind(sort) != shape_kind::pair) {
    if (events.empty()) return op_table::id;
    support_algebra& algebra = mgr.algebra(sort);
    code fused = events.front();
    for (const code e : events.subspan(1)) fused = algebra.term_sum(fused, e);
    return fused;
  }

  std::vector<code> flat;
  for (const code e : events) flatten_into(ops, e, flat);

  // The same split as the saturation rewrite, producing a sum instead of
  // a schedule: one-sided wrappers fold per side, recursively.
  std::vector<code> below, edge, rest;
  for (const code e : flat) {
    if (e == op_table::id) {
      rest.push_back(e);
      continue;
    }
    const op_term& t = ops[e];
    if (t.kind == op_kind::node) {
      if (t.operand(0) == op_table::id) {
        below.push_back(t.operand(1));
        continue;
      }
      if (t.operand(1) == op_table::id) {
        edge.push_back(t.operand(0));
        continue;
      }
    }
    rest.push_back(e);
  }
  if (!below.empty())
    rest.push_back(
        ops.node(op_table::id, sum_at(mgr, shapes.tail(sort), below)));
  if (!edge.empty())
    rest.push_back(
        ops.node(sum_at(mgr, shapes.head(sort), edge), op_table::id));
  return ops.sum(rest);
}

code saturate(manager& mgr, shape_code sort, std::span<const code> events) {
  if (events.empty()) return op_table::id;

  const shape_table& shapes = mgr.shapes();
  op_table& ops = mgr.operations();

  // Bottom of the recursion: a leaf closes its own maximal local term
  //  The theory decides how — fusing it, or iterating.
  if (shapes.kind(sort) != shape_kind::pair) {
    support_algebra& algebra = mgr.algebra(sort);
    code fused = events.front();
    for (const code e : events.subspan(1)) fused = algebra.term_sum(fused, e);
    return algebra.term_lfp(fused);
  }

  // A folded (or named) sum stands for its summands: flatten before the
  // split, so a `sum_at` system schedules identically to its flat list.
  std::vector<code> flat;
  for (const code e : events) flatten_into(ops, e, flat);

  // Split by where each summand reaches relative to this cut.
  // Our terms mirror the shape, so this is inspection, not an oracle.
  std::vector<code> below;   // F: tail only  — the tail terms themselves
  std::vector<code> edge;    // L: head only  — the head terms themselves
  std::vector<code> across;  // G: both       — kept whole, chained here
  for (const code e : flat) {
    if (e == op_table::id) continue;  // lfp is reflexive already
    const op_term& t = ops[e];
    if (t.kind == op_kind::node) {
      const code head = t.operand(0);
      const code tail = t.operand(1);
      if (head == op_table::id) {
        below.push_back(tail);
        continue;
      }
      if (tail == op_table::id) {
        edge.push_back(head);
        continue;
      }
    }
    across.push_back(e);
  }

  // F and L are closed recursively, so hierarchy is automatic: the two
  // child saturations are this same procedure one level in.
  const code f_part =
      below.empty()
          ? op_table::id
          : ops.node(op_table::id, saturate(mgr, shapes.tail(sort), below));
  const code l_part =
      edge.empty()
          ? op_table::id
          : ops.node(saturate(mgr, shapes.head(sort), edge), op_table::id);

  if (f_part == op_table::id && l_part == op_table::id) {
    // Nothing reaches past this cut on its own: there is no schedule to
    // exploit, so say so plainly rather than build a degenerate one.
    return ops.lfp(ops.sum(flat));
  }

  return ops.saturate(f_part, l_part, across);
}

// --- composition -----------------------------------------------------------

namespace {
/// The fused composition, or throws `unsupported_error` where a leaf refuses.
code compose_fused(manager& mgr, shape_code sort, code after, code before) {
  op_table& ops = mgr.operations();
  if (before == op_table::id) return after;
  if (after == op_table::id) return before;
  const shape_table& shapes = mgr.shapes();
  if (shapes.kind(sort) != shape_kind::pair) {
    return mgr.algebra(sort).term_compose(after, before);
  }
  const op_term& a = ops[after];
  const op_term& b = ops[before];
  if (a.kind == op_kind::sum) {
    std::vector<code> parts;
    for (const code s : a.operands()) parts.push_back(compose_fused(mgr, sort, s, before));
    return ops.sum(parts);
  }
  if (b.kind == op_kind::sum) {
    std::vector<code> parts;
    for (const code s : b.operands()) parts.push_back(compose_fused(mgr, sort, after, s));
    return ops.sum(parts);
  }
  if (a.kind == op_kind::node && b.kind == op_kind::node) {
    const code h = compose_fused(mgr, shapes.head(sort), a.operand(0), b.operand(0));
    const code t = compose_fused(mgr, shapes.tail(sort), a.operand(1), b.operand(1));
    return ops.node(h, t);
  }
  throw unsupported_error("no structural composition for these terms");
}
}  // namespace

code compose_at(manager& mgr, shape_code sort, code after, code before) {
  try {
    return compose_fused(mgr, sort, after, before);
  } catch (const unsupported_error&) {
    return mgr.operations().compose(after, before);
  }
}

// --- inversion -------------------------------------------------------------

code inverter::operator()(shape_code sort, code term, code potential) {
  if (term == op_table::id) return op_table::id;
  op_table& ops = mgr_.operations();
  const shape_table& shapes = mgr_.shapes();
  const bool leaf = shapes.kind(sort) != shape_kind::pair;
  // Nothing to come from: the zero term — the theory's at a leaf.
  if (potential == none && !leaf) return ops.within(none);

  const key k{sort, term, potential};
  if (const auto it = memo_.find(k); it != memo_.end()) return it->second;

  code result = none;
  if (leaf) {
    // A leaf: the theory inverts its own term against its domain.
    result = mgr_.algebra(sort).invert_local(term, potential);
  } else {
    // A reference: interned terms live on the heap and do not move when the
    // table grows, and the operands trail the header (a copy would lose them).
    const op_term& t = ops[term];
    diagram_engine& diagrams = mgr_.diagrams();
    switch (t.kind) {
      case op_kind::node: {
        // The projections of the potential: join of primes, join of subs.
        const shape_code hs = shapes.head(sort);
        const shape_code ts = shapes.tail(sort);
        support_algebra& head = mgr_.algebra(hs);
        support_algebra& tail = mgr_.algebra(ts);
        code ph = none;
        code pt = none;
        for (const arc& a : diagrams.arcs(potential)) {
          ph = ph == none ? a.prime : head.join(ph, a.prime);
          pt = pt == none ? a.sub : tail.join(pt, a.sub);
        }
        result = ops.node((*this)(hs, t.operand(0), ph),
                          (*this)(ts, t.operand(1), pt));
        break;
      }
      case op_kind::sum: {
        std::vector<code> inv;
        inv.reserve(t.arity);
        for (const code s : t.operands()) inv.push_back((*this)(sort, s, potential));
        result = ops.sum(inv);
        break;
      }
      case op_kind::compose: {
        // (a ∘ b)⁻¹_P = b⁻¹_P ∘ a⁻¹_{b(P)}: the left factor sees the words
        // the right one produces, which need not be states.
        const code a = t.operand(0);
        const code b = t.operand(1);
        const code mid = diagrams.apply_local(b, potential);
        result = ops.compose((*this)(sort, b, potential), (*this)(sort, a, mid));
        break;
      }
      case op_kind::lfp:
        result = ops.lfp((*this)(sort, t.operand(0), potential));
        break;
      case op_kind::within:
        result = term;  // a selector is self-converse
        break;
      case op_kind::expr:
        // A case bracket: guard only (arity 1) is a selector, self-converse;
        // one that assigns has no converse spelled here yet.
        if (t.arity != 1) {
          throw unsupported_error(
              "no converse for a case bracket that assigns across a cut");
        }
        result = term;
        break;
      case op_kind::gfp:
        throw unsupported_error("no converse for a deflationary closure");
      case op_kind::saturate:
        throw unsupported_error(
            "a saturated schedule is not inverted: invert its events");
    }
  }
  memo_.emplace(k, result);
  return result;
}

}  // namespace hsc::core
