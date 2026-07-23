/// \file manager.hh
/// \brief Owner of every table and cache. There are no singletons.
///
/// libsdd's design, and the reason for it: state reachable from a global is
/// state nobody can scope, replace, or run two of. Everything here is owned
/// by one object and reaches the code that uses it as an explicit argument.
#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "hsc/core/diagram.hh"
#include "hsc/core/operation.hh"
#include "hsc/core/shape.hh"
#include "hsc/core/support.hh"

namespace hsc::core {

/// \brief A libHSC universe: shapes, imported theories, diagrams.
class manager {
 public:
  manager() : diagrams_(std::make_unique<diagram_engine>(*this)) {}

  manager(const manager&) = delete;
  manager& operator=(const manager&) = delete;

  [[nodiscard]] shape_table& shapes() noexcept { return shapes_; }
  [[nodiscard]] const shape_table& shapes() const noexcept { return shapes_; }

  /// \brief Import a leaf theory; the returned index names it in a shape.
  template <typename Theory, typename... Args>
  std::pair<theory_index, Theory&> import(Args&&... args) {
    auto owned = std::make_unique<Theory>(std::forward<Args>(args)...);
    Theory& ref = *owned;
    theories_.push_back(std::move(owned));
    return {static_cast<theory_index>(theories_.size() - 1), ref};
  }

  [[nodiscard]] support_algebra& theory(theory_index i) {
    return *theories_[i];
  }

  /// \brief The algebra of the sort \p s: an imported theory, or diagrams.
  ///
  /// Internalisation (Thm 3.5) is exactly this function having two branches
  /// that return the same interface.
  /// Const because it is a lookup: what it hands back is not const, since an
  /// algebra interns as it works.
  [[nodiscard]] support_algebra& algebra(shape_code s) const {
    return shapes_.is_leaf(s) ? *theories_[shapes_.head(s)]
                              : static_cast<support_algebra&>(*diagrams_);
  }

  [[nodiscard]] diagram_engine& diagrams() noexcept { return *diagrams_; }

  /// The interned operation terms.
  [[nodiscard]] op_table& operations() noexcept { return operations_; }
  [[nodiscard]] const op_table& operations() const noexcept {
    return operations_;
  }

  /// \brief The §7 case engine evaluating `op_kind::expr` terms. Registered
  /// by its owner (it outlives nothing here — the caller keeps it alive);
  /// applying an expr term with none registered is a logic error.
  void set_cases(case_evaluator* cases) noexcept { cases_ = cases; }
  [[nodiscard]] case_evaluator* cases() const noexcept { return cases_; }

 private:
  shape_table shapes_;
  op_table operations_;
  std::vector<std::unique_ptr<support_algebra>> theories_;
  std::unique_ptr<diagram_engine> diagrams_;
  case_evaluator* cases_ = nullptr;
};

}  // namespace hsc::core
