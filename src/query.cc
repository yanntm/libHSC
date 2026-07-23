/// \file query.cc
/// \brief `x ⋈ y` across a cut (§7), by split_equiv on the head plus a curried
/// per-position restriction of the tail.

#include "hsc/query.hh"

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"
#include "hsc/core/shape.hh"
#include "hsc/leaves/int_set.hh"

namespace hsc {

namespace {

using core::code;
using core::shape_code;

/// The curried residual once one side is the known value `thr`: keep leaf
/// elements w with `thr op w` (value on the left) or `w op thr`.
struct residual {
  cmp op;
  std::int32_t thr;
  bool thr_left;
};

/// Keep the elements of a single leaf code that satisfy \p r.
code restrict_leaf(leaves::int_set_theory& theory, code leaf,
                   const residual& r) {
  std::vector<std::int32_t> out;
  for (const std::int32_t v : theory.elements(leaf)) {
    if (r.thr_left ? eval(r.op, r.thr, v) : eval(r.op, v, r.thr)) {
      out.push_back(v);
    }
  }
  return theory.of(out);
}

/// Restrict \p c so the value at frontier position \p pos satisfies the
/// per-position bound — the curried residual, which is separable and so a
/// plain descent to that leaf followed by a filter.
code restrict(core::manager& mgr, leaves::int_set_theory& theory,
              shape_code sort, code c, std::size_t pos, const residual& r) {
  if (c == core::none) return core::none;
  const core::shape_table& shapes = mgr.shapes();
  if (shapes.kind(sort) == core::shape_kind::leaf) {
    return restrict_leaf(theory, c, r);
  }
  core::diagram_engine& diagrams = mgr.diagrams();
  const std::size_t wh = shapes.width(shapes.head(sort));
  std::vector<core::arc> rects;
  for (const core::arc& a : diagrams.arcs(c)) {
    if (pos < wh) {
      const code np = restrict(mgr, theory, shapes.head(sort), a.prime, pos, r);
      if (np != core::none) rects.push_back({np, a.sub});
    } else {
      const code ns =
          restrict(mgr, theory, shapes.tail(sort), a.sub, pos - wh, r);
      if (ns != core::none) rects.push_back({a.prime, ns});
    }
  }
  return diagrams.canonize(sort, rects);
}

/// Restrict \p c so the value at frontier position \p pos lies in \p set —
/// the same descent as `restrict`, the filter a meet instead of a predicate.
code restrict_set(core::manager& mgr, leaves::int_set_theory& theory,
                  shape_code sort, code c, std::size_t pos, code set) {
  if (c == core::none) return core::none;
  const core::shape_table& shapes = mgr.shapes();
  if (shapes.kind(sort) == core::shape_kind::leaf) {
    return theory.meet(c, set);
  }
  core::diagram_engine& diagrams = mgr.diagrams();
  const std::size_t wh = shapes.width(shapes.head(sort));
  std::vector<core::arc> rects;
  for (const core::arc& a : diagrams.arcs(c)) {
    if (pos < wh) {
      const code np =
          restrict_set(mgr, theory, shapes.head(sort), a.prime, pos, set);
      if (np != core::none) rects.push_back({np, a.sub});
    } else {
      const code ns =
          restrict_set(mgr, theory, shapes.tail(sort), a.sub, pos - wh, set);
      if (ns != core::none) rects.push_back({a.prime, ns});
    }
  }
  return diagrams.canonize(sort, rects);
}

code select(core::manager& mgr, leaves::int_set_theory& theory, shape_code sort,
            code c, std::size_t xpos, cmp op, std::size_t ypos) {
  if (c == core::none) return core::none;
  const core::shape_table& shapes = mgr.shapes();
  core::diagram_engine& diagrams = mgr.diagrams();

  // Only pairs are reachable with two distinct positions.
  const std::size_t wh = shapes.width(shapes.head(sort));
  const bool x_head = xpos < wh;
  const bool y_head = ypos < wh;
  std::vector<core::arc> rects;

  if (x_head && y_head) {  // both above the cut: recurse into the head
    for (const core::arc& a : diagrams.arcs(c)) {
      const code np =
          select(mgr, theory, shapes.head(sort), a.prime, xpos, op, ypos);
      if (np != core::none) rects.push_back({np, a.sub});
    }
    return diagrams.canonize(sort, rects);
  }
  if (!x_head && !y_head) {  // both below: recurse into the tail
    for (const core::arc& a : diagrams.arcs(c)) {
      const code ns = select(mgr, theory, shapes.tail(sort), a.sub, xpos - wh,
                             op, ypos - wh);
      if (ns != core::none) rects.push_back({a.prime, ns});
    }
    return diagrams.canonize(sort, rects);
  }

  // The crossing cut: one coordinate in the head, the other in the tail. The
  // head side must be the leaf itself (deep-head currying is a later step).
  const bool head_is_x = x_head;
  const shape_code head = shapes.head(sort);
  const shape_code tail = shapes.tail(sort);
  if (!shapes.is_leaf(head)) {
    throw std::logic_error(
        "select_compare: head-side coordinate is not a leaf; deep-head "
        "currying needs split_equiv on diagrams");
  }
  const std::size_t tail_pos = (head_is_x ? ypos : xpos) - wh;

  for (const core::arc& a : diagrams.arcs(c)) {
    // Split the head coordinate; each class is a single value v. If the head
    // holds x, the residual on the tail is `v op y`; else it is `x op v`.
    for (const auto& [v, piece] : theory.split_equiv(a.prime)) {
      const code rsub = restrict(mgr, theory, tail, a.sub, tail_pos,
                                 {op, v, /*thr_left=*/head_is_x});
      if (rsub != core::none) rects.push_back({piece, rsub});
    }
  }
  return diagrams.canonize(sort, rects);
}

}  // namespace

code select_compare(core::manager& mgr, leaves::int_set_theory& theory,
                    shape_code sort, code diagram, std::size_t xpos, cmp op,
                    std::size_t ypos) {
  if (xpos == ypos) {  // x op x: reflexivity decides, no split needed
    return eval(op, 0, 0) ? diagram : core::none;
  }
  return select(mgr, theory, sort, diagram, xpos, op, ypos);
}

code select_in(core::manager& mgr, leaves::int_set_theory& theory,
               shape_code sort, code diagram, std::size_t pos, code set) {
  return restrict_set(mgr, theory, sort, diagram, pos, set);
}

}  // namespace hsc
