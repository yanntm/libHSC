/// \file operation.cc
/// \brief Term construction, event assembly, and the saturation rewrite.
///
/// The rewrite is re-expressed from libsdd's `sdd/hom/rewrite.hh` (the static
/// pass) and libDDD's `Add::skip_variable` partition cache in `ddd/SHom.cpp`;
/// the evaluation schedule it produces is in `diagram.cc`. libDDD's
/// `demo/hanoi/hanoiHom.cpp` is the same schedule hand-written, before it had
/// a name.

#include "hsc/core/operation.hh"

#include <algorithm>
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
    return algebra.term_closure(fused);
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
    if (e == op_table::id) continue;  // id is in every closure already
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
    std::vector<code> all(flat.begin(), flat.end());
    all.push_back(op_table::id);
    return ops.fixpoint(ops.sum(all));
  }

  return ops.saturate(f_part, l_part, across);
}

}  // namespace hsc::core
