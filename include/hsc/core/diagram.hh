/// \file diagram.hh
/// \brief Diagrams (§3): the normal form, and Construction 3.3 that builds it.
///
/// One diagram type, not two. A prime over a composite head *is* a diagram
/// over that head (Theorem 3.5), so `diagram_engine` implements
/// `support_algebra` like any leaf theory — Corollary 3.6 made structural.
///
/// See `algorithm.md` §4–§7.
#pragma once

#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

#include "hsc/core/shape.hh"
#include "hsc/core/support.hh"
#include "hsc/mem/cache.hh"
#include "hsc/mem/intern.hh"
#include "hsc/util/hash.hh"

namespace hsc::core {

class manager;
class diagram_engine;

/// One arc of a node: a prime (an equivalence class of the cut) and the sub
/// it leads to (the class's name).
struct arc {
  code prime;
  code sub;
  friend bool operator==(const arc&, const arc&) = default;
};

/// \brief A node: its sort, then its arcs in the same allocation.
///
/// The sort is stored because codes are position-relative (§2.6): the same
/// arc list under two sorts means two different things, and its primes would
/// be codes in two different algebras. Arcs are kept sorted by prime, which
/// is what makes the interned representation canonical — so equality of
/// diagrams is equality of ids.
struct node {
  shape_code sort;
  std::uint32_t arity;

  [[nodiscard]] const arc* data() const {
    return reinterpret_cast<const arc*>(this + 1);
  }
  [[nodiscard]] std::span<const arc> arcs() const { return {data(), arity}; }

  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_value(sort);
    for (const arc& a : arcs()) {
      util::hash_combine(seed, a.prime);
      util::hash_combine(seed, a.sub);
    }
    return seed;
  }
  friend bool operator==(const node& a, const node& b) {
    return a.sort == b.sort && std::ranges::equal(a.arcs(), b.arcs());
  }
};

/// \brief A binary operation of the set algebra, as a cache key.
///
/// The operation object *is* the key and computes its own result: the shape
/// `mem::cache` wants. Join and meet normalise their operands, so `a op b`
/// and `b op a` are one entry.
struct binary_op {
  enum class kind : std::uint8_t { join, meet, minus, apply };

  kind op;
  code a;
  code b;

  binary_op(kind k, code x, code y) : op(k), a(x), b(y) {
    // join and meet are commutative: one entry serves both orders.
    if ((op == kind::join || op == kind::meet) && a > b) std::swap(a, b);
  }

  friend bool operator==(const binary_op&, const binary_op&) = default;
  [[nodiscard]] std::size_t hash() const {
    return util::hash_all(static_cast<std::uint8_t>(op), a, b);
  }
  code operator()(diagram_engine& engine) const;
};

/// \brief The diagram theory: the normal form, its canonicalizer, its algebra.
class diagram_engine final : public support_algebra {
 public:
  /// \param owner supplies the shape table and the leaf theories.
  /// \param cache_capacity how many binary results are memoized.
  explicit diagram_engine(manager& owner, std::size_t cache_capacity = 1u << 20);

  /// The terminal: the unique node of sort `1`, with no arcs.
  [[nodiscard]] code one() const noexcept { return one_; }

  /// \brief Construction 3.3: the one canonicalizer.
  ///
  /// Takes any finite bag of rectangles — overlapping, repeating, containing
  /// zeros — at sort \p sort, and returns the unique normal form. Every
  /// diagram in libHSC is built by this function.
  code canonize(shape_code sort, std::span<const arc> rectangles);

  /// \brief The diagram `P ⊗ S` at sort \p sort: one rectangle.
  code rectangle(shape_code sort, code prime, code sub);

  /// \name The support algebra (§2.1, tier G)
  ///@{
  code join(code a, code b) override;
  code meet(code a, code b) override;
  code minus(code a, code b) override;
  /// Apply an operation term (see `operation.hh`) to a diagram. This is the
  /// diagram theory answering Def 2.3 for itself — Corollary 3.6.
  code apply_local(code term, code value) override;
  /// Terms at a composite sort are operation terms, so these delegate to the
  /// operation table. The *saturating* closure needs to know the sort and is
  /// therefore `core::saturate` in `operation.hh`, not this.
  code term_sum(code a, code b) override;
  code term_closure(code t) override;
  [[nodiscard]] double cardinal(code c) const override;
  void print(std::ostream& os, code c) const override;
  ///@}

  /// \name Inspection
  ///@{
  [[nodiscard]] std::span<const arc> arcs(code c) const {
    return c == none ? std::span<const arc>{} : nodes_[c].arcs();
  }
  [[nodiscard]] shape_code sort_of(code c) const { return nodes_[c].sort; }
  [[nodiscard]] std::size_t node_count() const noexcept {
    return nodes_.size();
  }
  [[nodiscard]] const mem::intern_statistics& node_stats() const {
    return nodes_.stats();
  }
  [[nodiscard]] const mem::cache_statistics& op_stats() const {
    return ops_.stats();
  }
  ///@}

  /// \brief Nodes reachable from \p c, counted once each: the real size.
  [[nodiscard]] std::size_t size(code c) const;

 private:
  friend struct binary_op;

  /// Arcs indexed by sub: the invariant (F) as a data structure.
  class accumulator;

  /// The algebra the primes of a node of sort \p sort live in.
  [[nodiscard]] support_algebra& head_algebra(shape_code sort) const;
  /// The algebra the subs of a node of sort \p sort live in.
  [[nodiscard]] support_algebra& tail_algebra(shape_code sort) const;

  code intern_node(shape_code sort, std::span<const arc> arcs);
  /// Drop dead entries, order canonically, intern.
  code finish(shape_code sort, accumulator& acc);
  /// Merge one operand of pairwise-disjoint arcs, keeping (D).
  void sieve(shape_code sort, accumulator& acc, std::span<const arc> operand);

  code do_join(code a, code b);
  code do_meet(code a, code b);
  code do_minus(code a, code b);
  code do_apply(code term, code value);

  void collect_nodes(code c, std::unordered_set<code>& seen) const;

  manager& owner_;
  mem::intern<node> nodes_;
  mem::cache<diagram_engine, binary_op> ops_;
  code one_ = none;
  mutable std::vector<double> cardinal_memo_;
};

}  // namespace hsc::core
