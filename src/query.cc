/// \file query.cc
/// \brief `x ⋈ y` across a cut, by split_equiv on the head plus a curried
/// per-position restriction of the tail.

#include "hsc/query.hh"

#include <cstdint>
#include <map>
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

/// Restrict \p c so the value at frontier position \p pos satisfies
/// \p leaf_op — the one descent every separable per-position criterion
/// shares; only the filter applied at the leaf differs.
template <class LeafOp>
code descend(core::manager& mgr, shape_code sort, code c, std::size_t pos,
             LeafOp&& leaf_op) {
  if (c == core::none) return core::none;
  const core::shape_table& shapes = mgr.shapes();
  if (shapes.kind(sort) == core::shape_kind::leaf) return leaf_op(c);
  core::diagram_engine& diagrams = mgr.diagrams();
  const std::size_t wh = shapes.width(shapes.head(sort));
  std::vector<core::arc> rects;
  for (const core::arc& a : diagrams.arcs(c)) {
    if (pos < wh) {
      const code np = descend(mgr, shapes.head(sort), a.prime, pos, leaf_op);
      if (np != core::none) rects.push_back({np, a.sub});
    } else {
      const code ns = descend(mgr, shapes.tail(sort), a.sub, pos - wh, leaf_op);
      if (ns != core::none) rects.push_back({a.prime, ns});
    }
  }
  return diagrams.canonize(sort, rects);
}

/// The curried residual as a per-position restriction.
code restrict(core::manager& mgr, leaves::int_set_theory& theory,
              shape_code sort, code c, std::size_t pos, const residual& r) {
  return descend(mgr, sort, c, pos,
                 [&](code leaf) { return restrict_leaf(theory, leaf, r); });
}

/// Partition \p c (sort \p sort) by the value at frontier position \p pos —
/// `split_equiv` one level in. A leaf is the theory's split; a pair splits the
/// side holding \p pos, pairs each class with the other side, and merges
/// classes by common value across arcs. Ascending by value, like the leaf.
std::vector<std::pair<std::int32_t, code>> split_position(
    core::manager& mgr, leaves::int_set_theory& theory, shape_code sort,
    code c, std::size_t pos) {
  const core::shape_table& shapes = mgr.shapes();
  if (shapes.kind(sort) == core::shape_kind::leaf) {
    return theory.split_equiv(c);
  }
  core::diagram_engine& diagrams = mgr.diagrams();
  const std::size_t wh = shapes.width(shapes.head(sort));
  std::map<std::int32_t, std::vector<core::arc>> classes;
  for (const core::arc& a : diagrams.arcs(c)) {
    if (pos < wh) {
      for (const auto& [v, piece] :
           split_position(mgr, theory, shapes.head(sort), a.prime, pos)) {
        classes[v].push_back({piece, a.sub});
      }
    } else {
      for (const auto& [v, piece] :
           split_position(mgr, theory, shapes.tail(sort), a.sub, pos - wh)) {
        classes[v].push_back({a.prime, piece});
      }
    }
  }
  std::vector<std::pair<std::int32_t, code>> out;
  out.reserve(classes.size());
  for (auto& [v, rects] : classes) {
    const code piece = diagrams.canonize(sort, rects);
    if (piece != core::none) out.emplace_back(v, piece);
  }
  return out;
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

  // The crossing cut: one coordinate in the head, the other in the tail.
  // Split the head side by its coordinate — the theory's split if it is the
  // leaf itself, split_position one level in if it sits deeper — then, per
  // class value v, curry the residual (`v op y`, or `x op v`) onto the tail.
  const bool head_is_x = x_head;
  const shape_code head = shapes.head(sort);
  const shape_code tail = shapes.tail(sort);
  const std::size_t head_pos = head_is_x ? xpos : ypos;
  const std::size_t tail_pos = (head_is_x ? ypos : xpos) - wh;

  for (const core::arc& a : diagrams.arcs(c)) {
    for (const auto& [v, piece] :
         split_position(mgr, theory, head, a.prime, head_pos)) {
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
  return descend(mgr, sort, diagram, pos,
                 [&](code leaf) { return theory.meet(leaf, set); });
}

code select_where(core::manager& mgr, leaves::int_set_theory& theory,
                  shape_code sort, code diagram, std::size_t pos,
                  lia::bexpr guard) {
  return descend(mgr, sort, diagram, pos,
                 [&](code leaf) { return theory.filter(leaf, guard); });
}

std::vector<std::pair<std::int32_t, code>> split_equiv(
    core::manager& mgr, leaves::int_set_theory& theory, shape_code sort,
    code diagram, std::size_t pos) {
  if (diagram == core::none) return {};
  return split_position(mgr, theory, sort, diagram, pos);
}

}  // namespace hsc
