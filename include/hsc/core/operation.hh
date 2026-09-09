/// \file operation.hh
/// \brief Operation terms, and the saturated form of a closure.
///
///     H ::= id | node(H_h, H_t) | H ∘ H | Σ H | lfp H | gfp H | within(D)
///         | saturate(F, L, G…)
///
/// `lfp h` is the least fixpoint `(id + h)*` — the derived form of the
/// theory contract's pure star, offered as the primitive so recognizing an
/// accumulation never requires matching operands. Bare star is deliberately
/// absent: nothing in the calculus asks for it yet. `gfp h` is its dual, the
/// deflationary closure `X ↦ X ∩ h(X)` iterated downward from the argument
/// (`algorithm.md` §8); a composite-sort term only, never pushed to a leaf.
/// `within(D)` is the constant selector `X ↦ X ∩ D` for a diagram `D` — a
/// diagram read as a term (`algorithm.md` §9).
///
/// The leaf case is not in this table: at a leaf sort the term is a *theory*
/// term, read by the theory that owns the sort. A term is
/// interpreted by whichever algebra it is handed to, exactly like a value.
///
/// The term mirrors the shape tree rather than naming an absolute variable.
/// Three consequences, all load-bearing:
///
///   **Skip is `term == id`.** `node(id, t)` transmits nothing to the head,
///   because `id` is free and free things are not represented. No skip
///   oracle, no support set.
///
///   **Currification is free**. Descending into a subtree re-roots
///   the term, so isomorphic positions share codes. libDDD keys a
///   homomorphism on an absolute variable index and structurally cannot.
///
///   **The saturation split is syntactic.** Where each summand reaches
///   relative to a cut, `node(h,t)` answers by inspection —
///   `h == id` is F, `t == id` is L, neither is G. libDDD computes this with
///   a per-variable partition cache over `skip_variable`, libsdd with a
///   `dynamic_cast` chain in a static rewrite pass.
#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "hsc/core/code.hh"
#include "hsc/core/shape.hh"
#include "hsc/mem/intern.hh"
#include "hsc/util/hash.hh"

namespace hsc::core {

enum class op_kind : std::uint8_t {
  node,      ///< 2 operands: the head term and the tail term
  sum,       ///< n operands, canonical (sorted, deduplicated)
  compose,   ///< 2 operands: `after ∘ before`
  lfp,       ///< 1 operand: the least fixpoint `(id + h)*`, by naive iteration
  gfp,       ///< 1 operand: the greatest fixpoint of `X ↦ X ∩ h(X)` below the argument
  within,    ///< 1 operand: a *diagram* code D; the constant selector `X ↦ X ∩ D`
  saturate,  ///< F, L, then the G operands: the F-L-G schedule
  expr,      ///< a case bracket: a guard `bexpr`, then (lhs, rhs)
             ///< `iexpr` pairs — opaque to core, evaluated by the case
             ///< engine registered with the manager
};

/// An operation term: a kind and its operands, in one allocation.
struct op_term {
  op_kind kind;
  std::uint32_t arity;

  [[nodiscard]] const code* data() const {
    return reinterpret_cast<const code*>(this + 1);
  }
  [[nodiscard]] std::span<const code> operands() const {
    return {data(), arity};
  }
  [[nodiscard]] code operand(std::size_t i) const { return data()[i]; }

  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_value(static_cast<std::uint8_t>(kind));
    util::hash_range(seed, data(), data() + arity);
    return seed;
  }
  friend bool operator==(const op_term& a, const op_term& b) {
    return a.kind == b.kind && std::ranges::equal(a.operands(), b.operands());
  }
};

/// \brief The interned operation terms.
class op_table {
 public:
  /// `id`: the empty composition. Never represented, never interned.
  static constexpr code id = none;

  /// Act by \p head on the primes and \p tail on the subs. Collapses to `id`
  /// when neither does anything, so skip trees build themselves.
  code node(code head, code tail) {
    if (head == id && tail == id) return id;
    const code ops[] = {head, tail};
    return make(op_kind::node, ops);
  }

  /// \brief The sum of \p operands, canonically ordered.
  ///
  /// Sum is commutative and idempotent, so sorting and deduplicating is a
  /// normal form — which is how two events written in different orders end
  /// up one code. `id` is a legitimate operand and is kept: it is what makes
  /// a closure reflexive.
  code sum(std::span<const code> operands) {
    scratch_.assign(operands.begin(), operands.end());
    std::ranges::sort(scratch_);
    const auto dup = std::ranges::unique(scratch_);
    scratch_.erase(dup.begin(), dup.end());
    if (scratch_.empty()) return id;
    if (scratch_.size() == 1) return scratch_.front();
    return make(op_kind::sum, scratch_);
  }

  code sum(code a, code b) {
    const code ops[] = {a, b};
    return sum(ops);
  }

  code compose(code after, code before) {
    if (after == id) return before;
    if (before == id) return after;
    const code ops[] = {after, before};
    return make(op_kind::compose, ops);
  }

  /// `lfp(h) = (id + h)*` by naive iteration: the unsaturated closure.
  code lfp(code h) {
    if (h == id) return id;
    const code ops[] = {h};
    return make(op_kind::lfp, ops);
  }

  /// `gfp(h)`: the deflationary closure, by iteration from the argument
  /// downward. `gfp(id)` is `id`: `X ∩ X` is `X`.
  code gfp(code h) {
    if (h == id) return id;
    const code ops[] = {h};
    return make(op_kind::gfp, ops);
  }
  /// `within(D)`: the constant selector `X ↦ X ∩ D`, the term reading of a
  /// diagram. Additive and self-converse. `within(none)` is the zero term.
  code within(code diagram) {
    const code ops[] = {diagram};
    return make(op_kind::within, ops);
  }
  /// \brief The saturation schedule: `(F + id)*`, then `(L + id)*`, then the
  /// `G` chain, to stability.
  ///
  /// \p f and \p l are already terms *at this sort* — `node(id,F')` and
  /// `node(L',id)` — so evaluation applies them without rewrapping.
  code saturate(code f, code l, std::span<const code> g) {
    scratch_.assign({f, l});
    scratch_.insert(scratch_.end(), g.begin(), g.end());
    return make(op_kind::saturate, scratch_);
  }

  /// \brief A case bracket: `when guard do lhs := rhs, …`.
  ///
  /// \p guard is a `lia::bexpr` code, \p assigns interleaved
  /// (lhs, rhs) `lia::iexpr` codes; every position in them is relative to
  /// the sort the term is applied at. Core stores and hashes the term but
  /// never reads the expressions — evaluation is the registered case
  /// engine's (the calculus carries the interchange theory's
  /// codes, the theories at the ends interpret them).
  code expr_event(code guard, std::span<const code> assigns) {
    scratch_.assign({guard});
    scratch_.insert(scratch_.end(), assigns.begin(), assigns.end());
    return make(op_kind::expr, scratch_);
  }

  [[nodiscard]] const op_term& operator[](code c) const { return table_[c]; }
  [[nodiscard]] std::size_t size() const noexcept { return table_.size(); }
  [[nodiscard]] const mem::intern_statistics& stats() const {
    return table_.stats();
  }

 private:
  code make(op_kind kind, std::span<const code> operands);

  mem::intern<op_term> table_;
  std::vector<code> scratch_;
};

/// \brief Assemble the term applying `by_leaf[i]` at the i-th leaf of \p sort.
///
/// How an event is written: every leaf gets its own maximal local term (`id`
/// where the event does not touch it) and the shape assembles them. A Petri
/// transition, or a Hanoi move, is exactly this — one traversal, and the
/// `id` subtrees are skipped because they are not there.
code product(op_table& ops, const shape_table& shapes, shape_code sort,
             std::span<const code> by_leaf);

class manager;

/// \brief The evaluator of `op_kind::expr` terms, registered with the
/// manager by the layer that owns the case bracket (`hsc/event.hh`).
///
/// Core dispatches here from term application; the separation keeps the
/// calculus free of any leaf theory or expression language.
class case_evaluator {
 public:
  virtual ~case_evaluator() = default;
  /// Apply the expr term \p term to \p diagram (nonzero, composite sort).
  virtual code apply(code term, code diagram) = 0;
};

/// \brief The sum of \p events at \p sort, in head-folded normal form.
///
/// Distributivity of sum over the head and tail applications gives two
/// sound folds:
///
///     node(A, id) ⊕ node(B, id) = node(A ⊕ B, id)
///     node(id, A) ⊕ node(id, B) = node(id, A ⊕ B)
///
/// Applied recursively per side, a family's site enumeration — one
/// wrapper chain per instance around one shared term — collapses into a
/// chain that mirrors the shape: O(depth) summands at every level where
/// the flat list held one per site. The mixed pair `node(A,id) ⊕
/// node(id,B)` does not fold; it, and every crossing term, stays a flat
/// summand. Sort-aware because a node over a leaf head holds *theory*
/// terms, whose sum is the theory's (`term_sum`), not the op table's.
/// Nested sums are flattened on entry, so the result is canonical.
code sum_at(manager& mgr, shape_code sort, std::span<const code> events);

/// \brief Rewrite a set of events into the saturated closure at \p sort.
///
/// The static saturation pass: partition the events by where they reach relative
/// to this cut, close F below and L on the edge — recursively, so hierarchy
/// is automatic — and leave G to be chained. Bottoms out at a leaf,
/// where the theory closes its own local term 
///
/// The result is a term; applying it is what saturates. Because the closures
/// sit *inside* the term, memoisation keys on saturated nodes rather than on
/// rounds — and that is the whole point.
code saturate(manager& mgr, shape_code sort, std::span<const code> events);

/// \brief `after ∘ before` at \p sort, fused where the laws of `algorithm.md`
/// §11 allow: componentwise through `node`, pointwise through sums, the
/// theory's `term_compose` at a leaf. Where a leaf refuses, the composition
/// stays an unfused `compose` term at the node above (correct, a straddler).
code compose_at(manager& mgr, shape_code sort, code after, code before);

/// \brief Inversion of terms relative to a potential (`algorithm.md` §9).
///
/// `operator()(sort, term, P)` is the term of the converse of \p term whose
/// results stay inside the potential `P` — a diagram at \p sort, or a theory
/// code at a leaf sort. Structural on `op_kind`: `node` inverts per side
/// against the projections of `P` (the join of its primes, of its subs),
/// `compose` reverses and re-potentialises its left factor with the image of
/// the right one, `sum` and `lfp` pointwise, a guard-only case bracket and
/// `within` are self-converse; a leaf hands its term to the theory's
/// `invert_local`. Throws `unsupported_error` for a case bracket that
/// assigns, a `saturate` schedule or a `gfp` (the caller inverts the flat
/// events). Memoised on `(sort, term, potential)` for the object's lifetime
/// — codes are sort-relative, so the sort is part of the key — and
/// isomorphic positions with equal projections share their inverse.
class inverter {
 public:
  explicit inverter(manager& mgr) : mgr_(mgr) {}
  code operator()(shape_code sort, code term, code potential);

 private:
  struct key {
    shape_code sort;
    code term;
    code potential;
    friend bool operator==(const key&, const key&) = default;
  };
  struct key_hash {
    std::size_t operator()(const key& k) const noexcept {
      std::size_t seed = util::hash_value(k.sort);
      util::hash_combine(seed, k.term);
      util::hash_combine(seed, k.potential);
      return seed;
    }
  };
  manager& mgr_;
  std::unordered_map<key, code, key_hash> memo_;
};

}  // namespace hsc::core
