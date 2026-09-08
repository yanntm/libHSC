/// \file eval.hh
/// \brief Concrete evaluation of `lia` expressions against one state.
///
/// Pure reading through the factory's const API — no interning traffic.
/// Arithmetic runs in int64, checked back into int32 at every fold step;
/// overflow is a loud error. ⊥ (division by zero, out-of-bounds array
/// read, bad shift, negative exponent) is Kleene inside a guard and an
/// error at any decision; the innermost cause is recorded so errors name
/// the culprit. Semantics in `algorithm.md` §3.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include "hsc/lia/expr.hh"
#include "hsc/xpl/state.hh"

namespace hsc::xpl {

/// A ⊥ that reached a decision, or an overflow: what and why, context-free.
/// `fire` wraps it into an `interp_error` bearing event and state.
class eval_error : public std::runtime_error {
 public:
  explicit eval_error(const std::string& cause)
      : std::runtime_error(cause) {}
};

/// Evaluation of one expression against one state. Stateless beyond the
/// recorded ⊥ cause; cheap to construct per use.
class evaluator {
 public:
  evaluator(const lia::expr_factory& ex, state_view env)
      : ex_(ex), env_(env) {}

  /// Three-valued guard evaluation (Kleene). On `undef` the innermost
  /// cause is available in `cause()`.
  [[nodiscard]] lia::expr_factory::truth guard(lia::bexpr e);

  /// Guard as a decision: `undef` throws `eval_error` with the cause.
  [[nodiscard]] bool decide(lia::bexpr e);

  /// Integer as a decision (an action rhs, a target index): ⊥ throws.
  [[nodiscard]] std::int64_t strict_int(lia::iexpr e);

  [[nodiscard]] const std::string& cause() const { return cause_; }

 private:
  /// ⊥ propagates via \p undef; the first cause is latched. Overflow
  /// throws immediately — it is loud even under Kleene.
  [[nodiscard]] std::int64_t eval_int(lia::iexpr e, bool& undef);
  [[nodiscard]] lia::expr_factory::truth eval_bool(lia::bexpr e);

  void bot(const std::string& why) {
    if (cause_.empty()) cause_ = why;
  }

  const lia::expr_factory& ex_;
  state_view env_;
  std::string cause_;
};

}  // namespace hsc::xpl
