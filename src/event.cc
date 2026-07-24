/// \file event.cc
/// \brief The case bracket: build (push-down) and apply (currying).
/// The algorithm is the file comment of `hsc/event.hh`.

#include "hsc/event.hh"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"
#include "hsc/leaves/int_set.hh"
#include "hsc/query.hh"

namespace hsc {

namespace {

using core::code;
using core::shape_code;

constexpr std::uint32_t no_read = std::numeric_limits<std::uint32_t>::max();

/// True when \p e is a node code (even, nonzero) of kind array.
bool is_array_node(const lia::expr_factory& ex, lia::iexpr e) {
  return (e & 1u) == 0 && e != 0 && ex.kind(e) == lia::ikind::array;
}

/// True when \p e is a node code (even, nonzero) — not an immediate.
bool is_node(std::uint32_t e) { return (e & 1u) == 0 && e != 0; }

/// \name The assertion chooser
///
/// A usable assertion subexpression for the side `[lo, hi)`: scalar support
/// wholly inside, containing the least read \p r, no array placements.
/// The walk returns the topmost such subterm — the maximal assertion — and
/// a bare `variable(r)` degenerates to the coordinate split.
///@{
bool assertable(lia::expr_factory& ex, lia::iexpr e, std::uint32_t lo,
                std::uint32_t hi, std::uint32_t r) {
  if (!is_node(e)) return false;
  if (!ex.array_positions(e).empty()) return false;
  const auto sup = ex.support(e);
  if (sup.empty()) return false;
  bool has = false;
  for (const std::uint32_t p : sup) {
    if (p < lo || p >= hi) return false;
    if (p == r) has = true;
  }
  return has;
}

lia::iexpr find_assert_b(lia::expr_factory& ex, lia::bexpr e, std::uint32_t lo,
                         std::uint32_t hi, std::uint32_t r);

lia::iexpr find_assert_i(lia::expr_factory& ex, lia::iexpr e, std::uint32_t lo,
                         std::uint32_t hi, std::uint32_t r) {
  if (!is_node(e)) return 0;
  if (assertable(ex, e, lo, hi, r)) return e;
  const lia::expr_node& n = ex.node(e);
  switch (ex.kind(e)) {
    case lia::ikind::array:  // only the index is an expression
      return find_assert_i(ex, n.operands()[0], lo, hi, r);
    case lia::ikind::wrap_bool:
      return find_assert_b(ex, n.operands()[0], lo, hi, r);
    default:
      for (const std::uint32_t op : n.operands()) {
        if (const lia::iexpr f = find_assert_i(ex, op, lo, hi, r)) return f;
      }
      return 0;
  }
}

lia::iexpr find_assert_b(lia::expr_factory& ex, lia::bexpr e, std::uint32_t lo,
                         std::uint32_t hi, std::uint32_t r) {
  if (!is_node(e)) return 0;
  const lia::expr_node& n = ex.bool_node(e);
  switch (ex.bool_kind(e)) {
    case lia::bkind::conj:
    case lia::bkind::disj:
    case lia::bkind::neg:
      for (const std::uint32_t op : n.operands()) {
        if (const lia::iexpr f = find_assert_b(ex, op, lo, hi, r)) return f;
      }
      return 0;
    default:  // a comparison: its sides are the topmost iexpr candidates
      for (const std::uint32_t op : n.operands()) {
        if (const lia::iexpr f = find_assert_i(ex, op, lo, hi, r)) return f;
      }
      return 0;
  }
}
///@}

}  // namespace

case_engine::case_engine(core::manager& mgr, leaves::int_set_theory& theory)
    : mgr_(mgr), theory_(&theory) {
  mgr.set_cases(this);
}

code case_engine::zero_term(shape_code sort) {
  const core::shape_table& shapes = mgr_.shapes();
  if (shapes.kind(sort) == core::shape_kind::leaf) {
    return theory_->keep_if(lia::bfalse);
  }
  return mgr_.operations().node(zero_term(shapes.head(sort)),
                                core::op_table::id);
}

code case_engine::make_event(shape_code sort, lia::bexpr guard,
                             std::span<const assign> assigns) {
  lia::expr_factory& ex = theory_->exprs();

  if (guard == lia::bfalse || guard == lia::bundef) return zero_term(sort);

  std::vector<assign> as;
  as.reserve(assigns.size());
  for (const assign& a : assigns) {
    const assign r = a;
    if (r.lhs == lia::iundef || r.rhs == lia::iundef) {
      return zero_term(sort);  // an out-of-bounds access aborts: the 0
    }
    // A write of a position to itself under a true guard is id; dropping it
    // here lets the whole event collapse when nothing else remains.
    if (r.lhs == r.rhs) continue;
    as.push_back(r);
  }
  if (guard == lia::btrue && as.empty()) return core::op_table::id;

  const core::shape_table& shapes = mgr_.shapes();
  if (shapes.kind(sort) == core::shape_kind::leaf) {
    // One coordinate: the expressions must be over position 0 alone.
    lia::iexpr rhs0 = 0;
    bool has = false;
    for (const assign& a : as) {
      if (a.lhs != ex.variable(0) || has) {
        throw std::logic_error("case bracket: leaf event is not one write");
      }
      has = true;
      rhs0 = a.rhs;
    }
    return has ? theory_->apply_if(guard, rhs0) : theory_->keep_if(guard);
  }

  // The touched positions decide the cut: reads (guard, rhs, array
  // indices), every cell an unresolved array could address, and write
  // targets.
  std::uint32_t lo = no_read;
  std::uint32_t hi = 0;  // exclusive
  const auto touch = [&](std::uint32_t p) {
    lo = std::min(lo, p);
    hi = std::max(hi, p + 1);
  };
  const auto touch_iexpr = [&](lia::iexpr e) {
    for (const std::uint32_t p : ex.support(e)) touch(p);
    for (const std::uint32_t p : ex.array_positions(e)) touch(p);
  };
  for (const std::uint32_t p : ex.support_bool(guard)) touch(p);
  for (const std::uint32_t p : ex.array_positions_bool(guard)) touch(p);
  for (const assign& a : as) {
    touch_iexpr(a.rhs);
    if (is_array_node(ex, a.lhs)) {
      touch_iexpr(a.lhs);  // every cell it could write, and its index reads
    } else {
      touch(static_cast<std::uint32_t>(ex.node(a.lhs).payload));
    }
  }

  const std::size_t wh = shapes.width(shapes.head(sort));
  if (hi <= wh) {  // wholly in the head: positions are already head-rooted
    return mgr_.operations().node(make_event(shapes.head(sort), guard, as),
                                  core::op_table::id);
  }
  if (lo >= wh) {  // wholly in the tail: re-root by -wh
    const auto delta = -static_cast<std::int32_t>(wh);
    lia::bexpr g = ex.shift_positions_bool(guard, delta);
    std::vector<assign> shifted;
    shifted.reserve(as.size());
    for (const assign& a : as) {
      shifted.push_back({ex.shift_positions(a.lhs, delta),
                         ex.shift_positions(a.rhs, delta)});
    }
    return mgr_.operations().node(
        core::op_table::id,
        make_event(shapes.tail(sort), g, shifted));
  }

  // Crossing at this cut: an ordinary G event, chained by saturation.
  std::ranges::sort(as, {}, [](const assign& a) {
    return std::pair{a.lhs, a.rhs};
  });
  std::vector<code> ops;
  ops.reserve(as.size() * 2);
  for (const assign& a : as) {
    ops.push_back(a.lhs);
    ops.push_back(a.rhs);
  }
  return mgr_.operations().expr_event(guard, ops);
}

code case_engine::apply(code term, code d) {
  const core::op_term& t = mgr_.operations()[term];
  lia::expr_factory& ex = theory_->exprs();
  const lia::bexpr guard = t.operand(0);
  std::vector<assign> as;
  as.reserve((t.arity - 1) / 2);
  for (std::uint32_t i = 1; i + 1 < t.arity; i += 2) {
    as.push_back({t.operand(i), t.operand(i + 1)});
  }

  core::diagram_engine& diagrams = mgr_.diagrams();
  const core::shape_table& shapes = mgr_.shapes();
  const shape_code sort = diagrams.sort_of(d);
  const std::size_t wh = shapes.width(shapes.head(sort));
  const shape_code hsort = shapes.head(sort);
  const shape_code tsort = shapes.tail(sort);

  // The least position still read. Cells were resolved at build, so reads
  // are scalar supports: the guard, each rhs, each unresolved lhs index.
  std::uint32_t r = no_read;
  const auto see = [&](std::span<const std::uint32_t> ps) {
    for (const std::uint32_t p : ps) r = std::min(r, p);
  };
  see(ex.support_bool(guard));
  for (const assign& a : as) {
    see(ex.support(a.rhs));
    if (is_array_node(ex, a.lhs)) see(ex.support(a.lhs));
  }

  if (r != no_read) {
    // Curry: split the side holding r by the maximal side-supported
    // subexpression around it (a bare variable degenerates to the
    // coordinate); per class marker, apply the assertion (e := m) to every
    // expression and hand the interned residual to the class's rectangle.
    const bool in_head = r < wh;
    const auto lo = static_cast<std::uint32_t>(in_head ? 0 : wh);
    const auto hi = static_cast<std::uint32_t>(
        in_head ? wh : shapes.width(sort));
    lia::iexpr e = find_assert_b(ex, guard, lo, hi, r);
    for (std::size_t i = 0; e == 0 && i < as.size(); ++i) {
      e = find_assert_i(ex, as[i].rhs, lo, hi, r);
      if (e == 0 && is_array_node(ex, as[i].lhs)) {
        e = find_assert_i(ex, as[i].lhs, lo, hi, r);
      }
    }
    if (e == 0) e = ex.variable(r);
    const lia::iexpr e_side =
        in_head ? e : ex.shift_positions(e, -static_cast<std::int32_t>(wh));
    code out = core::none;
    for (const core::arc& a : diagrams.arcs(d)) {
      const auto classes =
          split_equiv(mgr_, *theory_, in_head ? hsort : tsort,
                      in_head ? a.prime : a.sub, e_side);
      for (const auto& [m, piece] : classes) {
        const lia::bexpr g = ex.subst_subexpr_bool(guard, e, m);
        if (g == lia::bfalse || g == lia::bundef) continue;
        std::vector<assign> curried;
        curried.reserve(as.size());
        bool aborted = false;
        for (const assign& x : as) {
          assign c{is_array_node(ex, x.lhs) ? ex.subst_subexpr(x.lhs, e, m)
                                            : x.lhs,
                   ex.subst_subexpr(x.rhs, e, m)};
          if (c.lhs == lia::iundef || c.rhs == lia::iundef) {
            aborted = true;  // out of bounds or ⊥ value: this class is 0
            break;
          }
          curried.push_back(c);
        }
        if (aborted) continue;
        const code residual = make_event(sort, g, curried);
        const code sub = diagrams.rectangle(sort, in_head ? piece : a.prime,
                                            in_head ? a.sub : piece);
        out = diagrams.join(out, diagrams.apply_local(residual, sub));
      }
    }
    return out;
  }

  // Ground: nothing is read, the guard is an immediate. It is `true` —
  // false/⊥ never intern (make_event folds them) — and every write is a
  // constant to a known position: apply each side's writes per arc.
  std::vector<assign> head_as;
  std::vector<assign> tail_as;
  for (const assign& a : as) {
    const auto p = static_cast<std::uint32_t>(ex.node(a.lhs).payload);
    if (p < wh) {
      head_as.push_back(a);
    } else {
      tail_as.push_back({ex.shift_positions(a.lhs, -static_cast<std::int32_t>(wh)),
                         a.rhs});
    }
  }
  const code th = make_event(hsort, lia::btrue, head_as);
  const code tt = make_event(tsort, lia::btrue, tail_as);
  std::vector<core::arc> bag;
  core::support_algebra& halg = mgr_.algebra(hsort);
  core::support_algebra& talg = mgr_.algebra(tsort);
  for (const core::arc& a : diagrams.arcs(d)) {
    const code np = halg.apply_local(th, a.prime);
    if (np == core::none) continue;
    const code ns = talg.apply_local(tt, a.sub);
    if (ns == core::none) continue;
    bag.push_back({np, ns});
  }
  return diagrams.canonize(sort, bag);
}

}  // namespace hsc
