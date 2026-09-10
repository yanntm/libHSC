/// \file formula.hh
/// \brief The CTL formula DAG: atoms, booleans, the ten path operators;
/// negation normal form and the existential dual. See `algorithm.md` §2.
///
/// Nodes are hash-consed: `(op, atom, kids)` is interned, so a subformula
/// shared by several properties is one node, and every memo keyed by node
/// id downstream is shared with it. Atoms are opaque indices into a table
/// the caller owns; the deadlock predicate is an atom like any other.
///
/// `and` / `or` are n-ary, flattened, deduplicated and constant-folded on
/// construction; `not` is kept only where the caller wrote it — `nnf`
/// pushes it to the atoms.
#pragma once
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace hsc::ctl {

/// The operators. Order matters: `ex` and above are path operators.
enum class op : std::uint8_t {
  atom,
  tru,
  fls,
  not_,
  and_,
  or_,
  ex,
  ax,
  ef,
  af,
  eg,
  ag,  // one child
  eu,
  au,
  ew,
  aw   // two children: left, right
};

[[nodiscard]] constexpr bool is_path(op o) noexcept { return o >= op::ex; }
[[nodiscard]] constexpr bool is_existential(op o) noexcept {
  return o == op::ex || o == op::ef || o == op::eg || o == op::eu ||
         o == op::ew;
}
[[nodiscard]] const char* name(op o) noexcept;

/// A node id: an index into the DAG. There is no absence value.
using node_id = std::uint32_t;

/// One node, as stored.
struct fnode {
  op kind;
  std::uint32_t atom;  ///< `op::atom` only
  std::vector<node_id> kids;
  friend bool operator==(const fnode&, const fnode&) = default;
};

/// \brief The DAG of formulas: construction with normalisation, the normal
/// forms, structural queries.
class formulas {
 public:
  node_id atom(std::uint32_t a);
  node_id constant(bool b);
  /// `¬f`, folding `¬¬f`, `¬true`, `¬false`.
  node_id negation(node_id f);
  /// `∧` / `∨` of \p kids: flattened, deduplicated, constants folded, an
  /// empty conjunction is `true`, an empty disjunction `false`, a singleton
  /// is its child.
  node_id conj(std::span<const node_id> kids);
  node_id disj(std::span<const node_id> kids);
  node_id conj(node_id a, node_id b) {
    const node_id k[] = {a, b};
    return conj(k);
  }
  node_id disj(node_id a, node_id b) {
    const node_id k[] = {a, b};
    return disj(k);
  }
  /// A path operator with one child (`EX AX EF AF EG AG`).
  node_id unary(op o, node_id f);
  /// A path operator with two children (`EU AU EW AW`), `left`, `right`.
  node_id binary(op o, node_id left, node_id right);

  [[nodiscard]] const fnode& operator[](node_id n) const { return nodes_[n]; }
  [[nodiscard]] std::size_t size() const noexcept { return nodes_.size(); }

  /// \brief Negation normal form: `not` only over atoms, pushed through
  /// the duals of `algorithm.md` §2. Idempotent.
  node_id nnf(node_id f);
  /// The negation normal form of `¬f`.
  node_id negate(node_id f);

  /// A state formula: no path operator anywhere below.
  [[nodiscard]] bool is_state(node_id f) const;
  /// \brief The path quantifiers of a formula in NNF: `existential` when
  /// every path operator is an E, `universal` when every one is an A,
  /// `none` for a state formula, `mixed` otherwise. On an under-approximated
  /// reachable set an existential formula answered TRUE stands, and so does
  /// a universal one answered FALSE; nothing else does (`algorithm.md` §7).
  enum class quantifiers : std::uint8_t { none, existential, universal, mixed };
  [[nodiscard]] quantifiers path_quantifiers(node_id f) const;
  /// \brief Convertible to forward form without putting a negation over a
  /// path operator (`algorithm.md` §4): a state formula, an existential
  /// node, or a conjunction / disjunction of convertibles. Expects NNF.
  [[nodiscard]] bool convertible(node_id f) const;

  /// Render \p f, atoms through \p atom_name.
  [[nodiscard]] std::string print(
      node_id f,
      const std::function<std::string(std::uint32_t)>& atom_name) const;

 private:
  node_id make(fnode n);
  node_id nary(op o, std::span<const node_id> kids);

  struct hasher {
    std::size_t operator()(const fnode& n) const noexcept;
  };
  std::vector<fnode> nodes_;
  std::unordered_map<fnode, node_id, hasher> index_;
  std::unordered_map<node_id, node_id> nnf_memo_;
  std::unordered_map<node_id, node_id> neg_memo_;
};

}  // namespace hsc::ctl
