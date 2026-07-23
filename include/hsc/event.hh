/// \file event.hh
/// \brief The §7 case bracket as an event: crossing criteria and updates.
///
/// An event is `when g do lhs_i := rhs_i` — `g` a `lia::bexpr`, both sides
/// of each assignment `lia::iexpr`s over frontier positions (an `lhs` is a
/// variable, or an array whose index is still an expression). The
/// separable fragment (§6) compiles to product terms elsewhere; this engine
/// owns what does not factor, by the operational model of the §7 bracket:
///
/// * **Build (`make_event`) pushes the term down.** At each sort, the
///   positions the event touches (reads: guard and rhs supports, array
///   index supports, the whole range of an array whose index is unresolved;
///   writes: lhs positions) decide the cut. All on one side: wrap
///   `node(·, id)` / `node(id, ·)` and recurse re-rooted (`shift_positions`
///   — array ids move with their cells). Both sides: intern an
///   `op_kind::expr` term at this sort — a `G` event, chained by the
///   ordinary saturation schedule. At a leaf: an `int_set` local term
///   (`keep_if` / `apply_if`).
///
/// * **Apply (`apply`) is currying.** Pick the least frontier position the
///   term still reads. Split the side holding it by that coordinate
///   (`split_equiv` — the theory's own split at a leaf, the bracket one
///   level in on diagrams); per class value, substitute it into every
///   expression and hand the residual — rebuilt by `make_event`, so it is
///   an interned term and its application is cached — to the class's
///   sub-diagram. When nothing is read anymore the guard is ground: `false`
///   or ⊥ contributes nothing (abort is the algebra's 0), `true` applies
///   the now-constant writes to each side. Federation is the join over
///   classes: merge by common key, no other machinery.
///
/// * **Cells are positions.** A resolved `arr[i]` is rewritten to the plain
///   variable `id + i` on entry (the id is cell 0's frontier position), so
///   only unresolved indices keep array nodes alive, and an array never
///   straddles a re-rooting: while unresolved it pins the term above its
///   whole range. An index that resolves out of bounds is ⊥ — the event
///   aborts, contributing absence, never a clamp.
///
/// This lives above `core` and `leaves` like `query.hh`: §7 is the seam
/// where the calculus consults a leaf theory's `split_equiv`; core carries
/// the expression codes without reading them and dispatches here through
/// `core::case_evaluator`.
#pragma once

#include <span>
#include <vector>

#include "hsc/core/code.hh"
#include "hsc/core/operation.hh"
#include "hsc/lia/expr.hh"

namespace hsc::core {
class manager;
}
namespace hsc::leaves {
class int_set_theory;
}

namespace hsc {

/// \brief Builder and evaluator of §7 crossing events.
class case_engine final : public core::case_evaluator {
 public:
  /// Registers itself with \p mgr as the `op_kind::expr` evaluator; the
  /// caller keeps this object alive as long as the manager runs terms.
  case_engine(core::manager& mgr, leaves::int_set_theory& theory);

  /// One assignment: \p lhs a variable or an array (whose index may still
  /// be an expression), \p rhs any integer expression. Positions are
  /// frontier positions of the sort the event is built at.
  struct assign {
    lia::iexpr lhs;
    lia::iexpr rhs;
  };

  /// \brief The event term for `when guard do assigns` at \p sort.
  ///
  /// The term sits at the deepest sort containing every touched position
  /// (see file comment); a guard of `false`/⊥, or a ⊥ side, folds to the
  /// zero term (a `false` filter — refuses every word). `id` when there is
  /// nothing to do.
  core::code make_event(core::shape_code sort, lia::bexpr guard,
                        std::span<const assign> assigns);

  /// Evaluate an `op_kind::expr` term against \p diagram (nonzero,
  /// composite sort) — the currying loop of the file comment.
  core::code apply(core::code term, core::code diagram) override;

 private:
  /// The term refusing every word at \p sort: `keep_if(false)` at the
  /// leftmost leaf. Abort returns the algebra's 0.
  core::code zero_term(core::shape_code sort);

  /// Rewrite every resolved cell `arr[i]` to the variable `arr + i`.
  lia::iexpr resolve_cells(lia::iexpr e);
  lia::bexpr resolve_cells_bool(lia::bexpr e);

  core::manager& mgr_;
  leaves::int_set_theory* theory_;
};

}  // namespace hsc
