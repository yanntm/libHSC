/// \file forward.hh
/// \brief The forward form: from `(seed, φ)` to a tree of emptiness
/// questions over set expressions. See `algorithm.md` §4–§5.
///
/// The conversion pushes the seed inward through the existential path
/// operators (`EX` to an image, `EU` to a forward closure, `EG` to the
/// forward SCC hull) so that the outer part of the formula never needs a
/// preimage. What it cannot push through — a universal operator under the
/// seed — becomes a `restrict` leaf: the forward set intersected with the
/// backward `Sat` of that subformula.
///
/// Both set expressions and questions are hash-consed tables, so the
/// evaluator memoises by id.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "hsc/ctl/formula.hh"

namespace hsc::ctl {

/// A set expression: how a forward set is built.
enum class set_op : std::uint8_t {
  init,      ///< the seed
  filter,    ///< `sel_f(r)`, `f` a state formula
  ey,        ///< `next(r)`
  fwdu,      ///< `lfp Z[r ∨ EY(Z ∧ q)]`: reached from `r` through `q`-states
  fwdg,      ///< nonempty iff some state of `r` satisfies `EG q`
  restrict_  ///< `r ∩ Sat(φ)`, `φ` evaluated backward
};

using set_id = std::uint32_t;

struct set_expr {
  set_op kind;
  set_id arg;  ///< the `r` operand; unused for `init`
  node_id f;   ///< the formula operand; unused for `init`, `ey`
  friend bool operator==(const set_expr&, const set_expr&) = default;
};

/// A question: a leaf asks whether a set is nonempty; `any` is the
/// disjunction of its children; `never` is the constant no.
enum class q_op : std::uint8_t { nonempty, any, never };

using q_id = std::uint32_t;

struct question {
  q_op kind;
  set_id set;  ///< `nonempty` only
  std::vector<q_id> kids;
  friend bool operator==(const question&, const question&) = default;
};

/// \brief The forward form of one property: the question tree, and whether
/// the verdict is the tree's answer or its negation (`algorithm.md` §5).
struct forward_form {
  q_id root;
  bool negated;  ///< the tree answers `I ∧ ¬φ ≠ ∅`; the verdict is its complement
};

/// \brief Converter: owns the set and question tables shared by every
/// property converted through it.
class forward {
 public:
  explicit forward(formulas& f) : f_(f) {}

  /// \brief Convert \p phi (any form; put in NNF here). With
  /// \p single_initial the polarity is chosen so the top is convertible;
  /// with several initial states the question is always `I ∧ ¬φ = ∅`.
  forward_form convert(node_id phi, bool single_initial = true);

  [[nodiscard]] const set_expr& set(set_id s) const { return sets_[s]; }
  [[nodiscard]] const question& q(q_id i) const { return qs_[i]; }
  [[nodiscard]] std::size_t set_count() const noexcept { return sets_.size(); }
  [[nodiscard]] std::size_t question_count() const noexcept {
    return qs_.size();
  }
  [[nodiscard]] formulas& forms() const noexcept { return f_; }

  /// Render a set expression or a question tree, atoms via \p atom_name.
  [[nodiscard]] std::string print_set(
      set_id s,
      const std::function<std::string(std::uint32_t)>& atom_name) const;
  [[nodiscard]] std::string print_q(
      q_id i,
      const std::function<std::string(std::uint32_t)>& atom_name) const;

 private:
  set_id mk_set(set_expr e);
  q_id mk_q(question q);
  q_id nonempty(set_id s);
  q_id any(std::vector<q_id> kids);
  /// The rules of §4 on `(r, phi)`, phi in NNF.
  q_id rule(set_id r, node_id phi);

  struct set_hasher {
    std::size_t operator()(const set_expr& e) const noexcept;
  };
  struct q_hasher {
    std::size_t operator()(const question& q) const noexcept;
  };
  formulas& f_;
  std::vector<set_expr> sets_;
  std::unordered_map<set_expr, set_id, set_hasher> set_index_;
  std::vector<question> qs_;
  std::unordered_map<question, q_id, q_hasher> q_index_;
};

}  // namespace hsc::ctl
