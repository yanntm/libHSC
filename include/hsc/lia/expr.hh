/// \file expr.hh
/// \brief Interned linear-integer-arithmetic expressions over positions.
///
/// The interchange theory: integer expressions (`iexpr`) and boolean
/// expressions (`bexpr`) as 32-bit codes. An even code names an interned
/// node; an odd code is an immediate — an inline constant in sign-magnitude
/// for `iexpr` (no node, no probe for the dominant population), the three
/// sentinels for `bexpr`. Factories normalize at construction: every held
/// code is in the normal form of `algorithm.md`, and substitution
/// renormalizes through the same factories. Adapted from the GAL expression
/// engine (libITS `its/gal`, positional layer).
#pragma once

#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

#include "hsc/mem/intern.hh"
#include "hsc/util/hash.hh"

namespace hsc::lia {

/// An integer-valued expression code. Even: node id << 1; odd: immediate.
using iexpr = std::uint32_t;
/// A boolean-valued expression code. Even: node id << 1; odd: immediate.
using bexpr = std::uint32_t;

/// ⊥, the undefined integer expression (array index out of bounds, division
/// by zero): the spare "-0" encoding of the inline-constant scheme.
inline constexpr iexpr iundef = 3;

/// The boolean immediates.
inline constexpr bexpr bfalse = 1;
inline constexpr bexpr btrue = 3;
inline constexpr bexpr bundef = 5;  ///< ⊥ as a boolean

/// Node kinds of an integer expression.
enum class ikind : std::uint8_t {
  constant,  ///< wide constant (|v| >= 2^30): payload holds the value
  var,       ///< a frontier position: payload holds the index
  plus,      ///< n-ary, operands sorted, duplicates kept
  mult,      ///< n-ary, operands sorted, duplicates kept
  minus,     ///< binary
  div,       ///< binary; /0 is ⊥
  mod,       ///< binary; %0 is ⊥
  pow,       ///< binary; negative exponent is ⊥
  bit_and,   ///< binary
  bit_or,    ///< binary
  bit_xor,   ///< binary
  bit_comp,  ///< unary ~
  lshift,    ///< binary; negative or >= 32 shift is ⊥
  rshift,    ///< binary; negative or >= 32 shift is ⊥
  array,     ///< payload packs (array id, limit); operand 0 the index
  cell,      ///< payload packs (array id, limit); operand 0 a constant index
  wrap_bool  ///< a bexpr as 0/1: operand 0 is the bexpr
};

/// Node kinds of a boolean expression.
enum class bkind : std::uint8_t {
  conj,  ///< n-ary and: operands sorted, deduplicated
  disj,  ///< n-ary or: operands sorted, deduplicated
  neg,   ///< unary not: survives only above conj/disj (comparisons flip)
  eq,    ///< operands sorted (commutative)
  neq,   ///< operands sorted (commutative)
  lt,    ///< as written: x < y and y > x are distinct codes, one meaning
  leq,
  gt,
  geq
};

/// \brief One expression node: kind, operand count, a payload, and the
/// operands as a trailing array. Shared layout for both kinds' tables.
struct expr_node {
  std::uint8_t kind;
  std::uint32_t count;
  std::int64_t payload;

  [[nodiscard]] const std::uint32_t* data() const {
    return reinterpret_cast<const std::uint32_t*>(this + 1);
  }
  [[nodiscard]] std::span<const std::uint32_t> operands() const {
    return {data(), count};
  }
  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_all(kind, count, payload);
    util::hash_range(seed, data(), data() + count);
    return seed;
  }
  friend bool operator==(const expr_node& a, const expr_node& b) {
    return a.kind == b.kind && a.count == b.count && a.payload == b.payload &&
           std::equal(a.data(), a.data() + a.count, b.data());
  }
};

/// \brief The two intern tables and every constructor. Owns its codes.
class expr_factory {
 public:
  // --- immediates and simple leaves ---------------------------------------

  /// The code of constant \p v: inline when |v| < 2^30, a wide node beyond.
  iexpr constant(std::int32_t v);
  /// The code for position \p pos.
  iexpr variable(std::uint32_t pos);

  [[nodiscard]] static bool is_const(iexpr e) {
    return (e & 1u) != 0 && e != iundef;
  }
  /// The value of a constant (inline or wide). Precondition: `is_const`,
  /// or a wide-constant node.
  [[nodiscard]] std::int32_t value(iexpr e) const;

  /// The kind of a non-immediate code. Immediates: `is_const` / == iundef.
  [[nodiscard]] ikind kind(iexpr e) const;
  [[nodiscard]] bkind bool_kind(bexpr e) const;
  [[nodiscard]] const expr_node& node(iexpr e) const;
  [[nodiscard]] const expr_node& bool_node(bexpr e) const;

  // --- integer constructors (normalizing) ----------------------------------

  iexpr add(std::span<const iexpr> xs);
  iexpr add(iexpr a, iexpr b);
  iexpr mul(std::span<const iexpr> xs);
  iexpr mul(iexpr a, iexpr b);
  iexpr sub(iexpr a, iexpr b);
  iexpr div(iexpr a, iexpr b);
  iexpr mod(iexpr a, iexpr b);
  iexpr pow(iexpr a, iexpr b);
  iexpr bit_and(iexpr a, iexpr b);
  iexpr bit_or(iexpr a, iexpr b);
  iexpr bit_xor(iexpr a, iexpr b);
  iexpr bit_comp(iexpr a);
  iexpr lshift(iexpr a, iexpr b);
  iexpr rshift(iexpr a, iexpr b);
  /// `arr[index]`, cells `[0, limit)`: a constant index resolves to `cell`
  /// (or ⊥ out of bounds).
  iexpr array(std::uint32_t arr, iexpr index, std::int32_t limit);
  /// A bexpr as the integer 0 or 1.
  iexpr wrap(bexpr b);

  // --- boolean constructors (normalizing) ----------------------------------

  bexpr compare(bkind op, iexpr l, iexpr r);
  bexpr conj(std::span<const bexpr> xs);
  bexpr conj(bexpr a, bexpr b);
  bexpr disj(std::span<const bexpr> xs);
  bexpr disj(bexpr a, bexpr b);
  bexpr neg(bexpr a);
  /// Drive every `neg` through conj/disj (De Morgan) to the comparisons.
  bexpr push_negations(bexpr a);

  // --- substitution: the currying step -------------------------------------

  /// Replace position \p pos by \p v, renormalizing. Grounded subterms fold;
  /// a fully grounded bexpr lands on btrue/bfalse/bundef.
  iexpr subst(iexpr e, std::uint32_t pos, iexpr v);
  bexpr subst_bool(bexpr e, std::uint32_t pos, iexpr v);
  /// Replace the array cell `arr[index]` by \p v.
  iexpr subst_cell(iexpr e, std::uint32_t arr, std::int32_t index, iexpr v);
  bexpr subst_cell_bool(bexpr e, std::uint32_t arr, std::int32_t index,
                        iexpr v);

  // --- evaluation ----------------------------------------------------------

  /// Three-valued: a guard can hold, fail, or be undefined (⊥).
  enum class truth : std::uint8_t { no, yes, undef };

  /// Evaluate with \p env giving the value at each position (positions
  /// beyond `env.size()` are an error the caller does not commit). Pure
  /// reading: no interning traffic, unlike `subst`. Array accesses evaluate
  /// to ⊥ — cells resolve by `subst_cell`, not through an env. Arithmetic
  /// overflow is loud.
  [[nodiscard]] truth eval_bool(bexpr e,
                                std::span<const std::int32_t> env) const;
  /// The integer twin; ⊥ is signalled by `undef` out-parameter.
  [[nodiscard]] std::int64_t eval_int(iexpr e,
                                      std::span<const std::int32_t> env,
                                      bool& undef) const;

  // --- reading -------------------------------------------------------------

  /// The scalar positions read, ascending, deduplicated. An array access
  /// contributes its index's support (which cells it reads is data).
  [[nodiscard]] std::vector<std::uint32_t> support(iexpr e) const;
  [[nodiscard]] std::vector<std::uint32_t> support_bool(bexpr e) const;

  /// The innermost non-constant nested expression (an array index), or
  /// \p e itself when nothing nests — the resolution order for
  /// `tab[tab[x]]`: curry x, then the inner access, then the outer.
  [[nodiscard]] iexpr first_subexpr(iexpr e) const;

  void print(std::ostream& os, iexpr e) const;
  void print_bool(std::ostream& os, bexpr e) const;

 private:
  iexpr intern(std::uint8_t k, std::int64_t payload,
               std::span<const std::uint32_t> ops);
  bexpr intern_bool(std::uint8_t k, std::int64_t payload,
                    std::span<const std::uint32_t> ops);
  iexpr nary(ikind k, std::span<const iexpr> xs);
  bexpr nary_bool(bkind k, std::span<const bexpr> xs);
  iexpr binary(ikind k, iexpr a, iexpr b);
  /// Fold \p k over two constant values; ⊥ (via `folded_undef`) for the
  /// partial cases. Overflow is loud.
  static std::int64_t fold(ikind k, std::int64_t l, std::int64_t r,
                           bool& undef);

  mem::intern<expr_node> inodes_;
  mem::intern<expr_node> bnodes_;
};

}  // namespace hsc::lia
