/// \file shape.hh
/// \brief Sorts: `V ::= 1 | ⟨A⟩ | (V_h , V_t)`.
///
/// Shapes are interned trees. Three commitments they encode:
///
///   No names.           A position is a path; two equal subterms are one
///                       object under hashing.
///   No associativity.   `(V1,(V2,V3))` and `((V1,V2),V3)` are distinct
///                       sorts. Refusing that quotient is what
///                       "hierarchical" means here.
///   The unit is mono-sorted.  `1` exists at one shape only.
#pragma once

#include <cstdint>

#include "hsc/core/code.hh"
#include "hsc/mem/intern.hh"
#include "hsc/util/hash.hh"

namespace hsc::core {

enum class shape_kind : std::uint8_t {
  unit,  ///< `1`: the terminal sort, no frontier, no position addresses it
  leaf,  ///< `⟨A⟩`: an imported theory
  pair,  ///< `(V_h, V_t)`
};

/// \brief One node of a shape tree.
struct shape {
  shape_kind kind = shape_kind::unit;
  /// leaf: the theory index. pair: the head sort. unit: unused.
  std::uint32_t head = 0;
  /// pair: the tail sort. otherwise unused.
  std::uint32_t tail = 0;

  friend bool operator==(const shape&, const shape&) = default;

  [[nodiscard]] std::size_t hash() const {
    return util::hash_all(static_cast<std::uint8_t>(kind), head, tail);
  }
};

/// \brief The interned shape trees, and the questions asked of a sort.
class shape_table {
 public:
  shape_table() : unit_(table_.get(shape{shape_kind::unit, 0, 0})) {}

  /// The terminal sort `1`.
  [[nodiscard]] shape_code unit() const noexcept { return unit_; }

  /// The leaf sort `⟨A⟩` over the theory registered at \p theory.
  [[nodiscard]] shape_code leaf(theory_index theory) {
    return table_.get(shape{shape_kind::leaf, theory, 0});
  }

  /// The composite sort `(head, tail)`.
  [[nodiscard]] shape_code pair(shape_code head, shape_code tail) {
    return table_.get(shape{shape_kind::pair, head, tail});
  }

  [[nodiscard]] const shape& operator[](shape_code s) const {
    return table_[s];
  }
  [[nodiscard]] shape_kind kind(shape_code s) const { return table_[s].kind; }
  [[nodiscard]] bool is_unit(shape_code s) const {
    return table_[s].kind == shape_kind::unit;
  }
  [[nodiscard]] bool is_leaf(shape_code s) const {
    return table_[s].kind == shape_kind::leaf;
  }

  /// The head sort of a pair; the theory index of a leaf.
  [[nodiscard]] shape_code head(shape_code s) const { return table_[s].head; }
  /// The tail sort of a pair.
  [[nodiscard]] shape_code tail(shape_code s) const { return table_[s].tail; }

  /// \brief Number of leaves, left to right: the width of a word of this sort.
  [[nodiscard]] std::size_t width(shape_code s) const {
    const shape& v = table_[s];
    switch (v.kind) {
      case shape_kind::unit:
        return 0;
      case shape_kind::leaf:
        return 1;
      case shape_kind::pair:
        return width(v.head) + width(v.tail);
    }
    return 0;
  }

  [[nodiscard]] std::size_t size() const noexcept { return table_.size(); }

 private:
  mem::intern<shape> table_;
  shape_code unit_;
};

}  // namespace hsc::core
