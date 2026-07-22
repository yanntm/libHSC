/// \file operation.hh
/// \brief Operation terms (§4.1), and the saturated form of a closure (§6).
///
///     H ::= id | node(H_h, H_t) | H ∘ H | Σ H | H* | saturate(F, L, G…)
///
/// The leaf case is not in this table: at a leaf sort the term is a *theory*
/// term, read by the theory that owns the sort (Def 2.3). A term is
/// interpreted by whichever algebra it is handed to, exactly like a value.
///
/// The term mirrors the shape tree rather than naming an absolute variable.
/// Three consequences, all load-bearing:
///
///   **Skip is `term == id`.** `node(id, t)` transmits nothing to the head,
///   because `id` is free and free things are not represented (discipline
///   3). No skip oracle, no support set.
///
///   **Currification is free** (§2.6). Descending into a subtree re-roots
///   the term, so isomorphic positions share codes. libDDD keys a
///   homomorphism on an absolute variable index and structurally cannot —
///   gap D1 of the bridge document.
///
///   **The saturation split is syntactic.** Def 6.1 asks where each summand
///   reaches relative to a cut; here `node(h,t)` answers by inspection —
///   `h == id` is F, `t == id` is L, neither is G. libDDD computes this with
///   a per-variable partition cache over `skip_variable`, libsdd with a
///   `dynamic_cast` chain in a static rewrite pass.
#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
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
  fixpoint,  ///< 1 operand: `(h + id)*` by naive iteration
  saturate,  ///< F, L, then the G operands: the schedule of §6.2
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

  /// `(h + id)*` by naive iteration: the unsaturated closure.
  code fixpoint(code h) {
    if (h == id) return id;
    const code ops[] = {h};
    return make(op_kind::fixpoint, ops);
  }

  /// \brief The schedule of §6.2: `(F + id)*`, then `(L + id)*`, then the
  /// `G` chain, to stability.
  ///
  /// \p f and \p l are already terms *at this sort* — `node(id,F')` and
  /// `node(L',id)` — so evaluation applies them without rewrapping.
  code saturate(code f, code l, std::span<const code> g) {
    scratch_.assign({f, l});
    scratch_.insert(scratch_.end(), g.begin(), g.end());
    return make(op_kind::saturate, scratch_);
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

/// \brief Rewrite a set of events into the saturated closure at \p sort.
///
/// The static pass of §6: partition the events by where they reach relative
/// to this cut, close F below and L on the edge — recursively, so hierarchy
/// is automatic (§6.4) — and leave G to be chained. Bottoms out at a leaf,
/// where the theory closes its own local term (§6.2, Def 2.3).
///
/// The result is a term; applying it is what saturates. Because the closures
/// sit *inside* the term, memoisation keys on saturated nodes rather than on
/// rounds — §6.5, and the whole point.
code saturate(manager& mgr, shape_code sort, std::span<const code> events);

}  // namespace hsc::core
