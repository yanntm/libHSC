/// \file event.cc
/// \brief The §7 case bracket: build (push-down) and apply (currying).
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

}  // namespace

case_engine::case_engine(core::manager& mgr, leaves::int_set_theory& theory)
    : mgr_(mgr), theory_(&theory) {
  mgr.set_cases(this);
}

lia::iexpr case_engine::resolve_cells(lia::iexpr e) {
  lia::expr_factory& ex = theory_->exprs();
  for (;;) {
    bool changed = false;
    for (const auto& r : ex.array_refs(e)) {
      if (r.index < 0) continue;
      e = ex.subst_cell(e, r.arr, r.index,
                        ex.variable(r.arr + static_cast<std::uint32_t>(
                                                r.index)));
      changed = true;
      break;  // refs are stale after a rewrite: rescan
    }
    if (!changed) return e;
  }
}

lia::bexpr case_engine::resolve_cells_bool(lia::bexpr e) {
  lia::expr_factory& ex = theory_->exprs();
  for (;;) {
    bool changed = false;
    for (const auto& r : ex.array_refs_bool(e)) {
      if (r.index < 0) continue;
      e = ex.subst_cell_bool(e, r.arr, r.index,
                             ex.variable(r.arr + static_cast<std::uint32_t>(
                                                     r.index)));
      changed = true;
      break;
    }
    if (!changed) return e;
  }
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

  // Cells are positions: resolve them before anything reads a support.
  guard = resolve_cells_bool(guard);
  if (guard == lia::bfalse || guard == lia::bundef) return zero_term(sort);

  std::vector<assign> as;
  as.reserve(assigns.size());
  for (const assign& a : assigns) {
    // resolve_cells on the lhs rewrites a resolved cell target to its
    // variable, and resolves cells inside an unresolved index; a plain
    // variable passes through.
    assign r{resolve_cells(a.lhs), resolve_cells(a.rhs)};
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
  // indices), the full range of any unresolved array, and write targets.
  std::uint32_t lo = no_read;
  std::uint32_t hi = 0;  // exclusive
  const auto touch = [&](std::uint32_t p, std::uint32_t n = 1) {
    lo = std::min(lo, p);
    hi = std::max(hi, p + n);
  };
  const auto touch_iexpr = [&](lia::iexpr e) {
    for (const std::uint32_t p : ex.support(e)) touch(p);
    for (const auto& r : ex.array_refs(e)) {
      touch(r.arr, static_cast<std::uint32_t>(r.limit));
    }
  };
  for (const std::uint32_t p : ex.support_bool(guard)) touch(p);
  for (const auto& r : ex.array_refs_bool(guard)) {
    touch(r.arr, static_cast<std::uint32_t>(r.limit));
  }
  for (const assign& a : as) {
    touch_iexpr(a.rhs);
    if (is_array_node(ex, a.lhs)) {
      touch_iexpr(a.lhs);  // range of the target array, and its index reads
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
    // Curry: split the side holding r by that coordinate; per class value,
    // substitute and hand the interned residual to the class's rectangle.
    const bool in_head = r < wh;
    code out = core::none;
    for (const core::arc& a : diagrams.arcs(d)) {
      const auto classes =
          split_equiv(mgr_, *theory_, in_head ? hsort : tsort,
                      in_head ? a.prime : a.sub,
                      in_head ? r : r - wh);
      for (const auto& [v, piece] : classes) {
        const lia::iexpr cv = ex.constant(v);
        const lia::bexpr g = ex.subst_bool(guard, r, cv);
        if (g == lia::bfalse || g == lia::bundef) continue;
        std::vector<assign> curried;
        curried.reserve(as.size());
        bool aborted = false;
        for (const assign& x : as) {
          assign c{is_array_node(ex, x.lhs) ? ex.subst(x.lhs, r, cv) : x.lhs,
                   ex.subst(x.rhs, r, cv)};
          if (c.lhs == lia::iundef || c.rhs == lia::iundef) {
            aborted = true;  // out of bounds: this class contributes 0
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
