/// \file operation.hh
/// \brief Operation terms (§4.1), the fragment that needs no query.
///
///     H ::= id | node(H_h, H_t) | H ∘ H | H + H
///
/// plus the leaf case, which is not in this table at all: at a leaf sort the
/// term is a *theory* term, interpreted by the theory that owns the sort
/// (Def 2.3). A term is therefore read by whichever algebra it is handed to,
/// exactly like a value.
///
/// The term mirrors the shape tree rather than naming an absolute variable.
/// Two consequences, both load-bearing:
///
///   **Skip is `term == id`.** `node(id, t)` transmits nothing to the head,
///   because `id` is free and free things are not represented (discipline
///   3). There is no skip oracle and no support set to compute.
///
///   **Currification is free** (§2.6). Descending into a subtree re-roots
///   the term: a guard on the first leaf of a subtree is one code, whatever
///   subtree that is and wherever it sits. libDDD keys a homomorphism on an
///   absolute variable index and structurally cannot share this — gap D1 of
///   the bridge document.
///
/// `∗` is absent: fixpoint arrives with saturation (M5).
#pragma once

#include <cstdint>
#include <span>

#include "hsc/core/code.hh"
#include "hsc/core/shape.hh"
#include "hsc/mem/intern.hh"
#include "hsc/util/hash.hh"

namespace hsc::core {

enum class op_kind : std::uint8_t {
  node,     ///< a: term for the head sort, b: term for the tail sort
  sum,      ///< a + b, applied to the same data
  compose,  ///< a ∘ b: b first
};

struct op_term {
  op_kind kind = op_kind::node;
  code a = none;
  code b = none;

  friend bool operator==(const op_term&, const op_term&) = default;
  [[nodiscard]] std::size_t hash() const {
    return util::hash_all(static_cast<std::uint8_t>(kind), a, b);
  }
};

/// \brief The interned operation terms.
class op_table {
 public:
  /// `id`: the empty composition. Never represented, never interned.
  static constexpr code id = none;

  /// The term acting by \p head on the primes and \p tail on the subs.
  /// Collapses to `id` when neither does anything — so skip trees build
  /// themselves and never need pruning.
  code node(code head, code tail) {
    if (head == id && tail == id) return id;
    return table_.get(op_term{op_kind::node, head, tail});
  }

  code sum(code a, code b) {
    if (a == id) return b == id ? id : table_.get(op_term{op_kind::sum, a, b});
    if (b == id) return table_.get(op_term{op_kind::sum, a, b});
    if (a == b) return a;
    return table_.get(op_term{op_kind::sum, a, b});
  }

  code compose(code after, code before) {
    if (after == id) return before;
    if (before == id) return after;
    return table_.get(op_term{op_kind::compose, after, before});
  }

  [[nodiscard]] const op_term& operator[](code c) const { return table_[c]; }
  [[nodiscard]] std::size_t size() const noexcept { return table_.size(); }
  [[nodiscard]] const mem::intern_statistics& stats() const {
    return table_.stats();
  }

 private:
  mem::intern<op_term> table_;
};

/// \brief Assemble the term applying `by_leaf[i]` at the i-th leaf of \p sort.
///
/// This is how an event is written: every leaf gets its own maximal local
/// term (`id` where the event does not touch it) and the shape assembles
/// them. A Petri transition, or a Hanoi move, is exactly this — no
/// composition needed, one traversal, and the `id` subtrees are skipped
/// because they are not there.
code product(op_table& ops, const shape_table& shapes, shape_code sort,
             std::span<const code> by_leaf);

}  // namespace hsc::core
