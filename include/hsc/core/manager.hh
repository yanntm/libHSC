/// \file manager.hh
/// \brief Owner of every table and cache. There are no singletons.
///
/// libsdd's design, and the reason for it: state reachable from a global is
/// state nobody can scope, replace, or run two of. Everything here is owned
/// by one object and reaches the code that uses it as an explicit argument.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <memory>
#include <utility>
#include <vector>

#include "hsc/core/diagram.hh"
#include "hsc/core/operation.hh"
#include "hsc/core/shape.hh"
#include "hsc/util/errors.hh"
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
    // the theory's long loops stop with the manager, returning what they have
    ref.set_stop_hooks({[this] { return stopping(); }, [this] { mark_partial(); }, [this] { request_stop(); }});
    theories_.push_back(std::move(owned));
    return {static_cast<theory_index>(theories_.size() - 1), ref};
  }

  [[nodiscard]] support_algebra& theory(theory_index i) {
    return *theories_[i];
  }

  /// \brief The algebra of the sort \p s: an imported theory, or diagrams.
  ///
  /// Internalisation is exactly this function having two branches
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

  /// \brief The crossing case engine evaluating `op_kind::expr` terms. Registered
  /// by its owner (it outlives nothing here — the caller keeps it alive);
  /// applying an expr term with none registered is a logic error.
  void set_cases(case_evaluator* cases) noexcept { cases_ = cases; }
  [[nodiscard]] case_evaluator* cases() const noexcept { return cases_; }
  /// \name Stopping a computation (`sched/algorithm.md` §3b)
  ///
  /// A stop is requested by a deadline or by a caller (a coordinator, a
  /// surface command); the closure loops consult `stopping()` once per
  /// round — a call per round, never per node — and return what they have,
  /// marking the result partial; nothing computed after the mark enters a
  /// cache (`cache_results()` of the engines); `reset_stop()` clears both
  /// at the root, before the next form. Computations that cannot use a
  /// partial value (a backward CTL set) call `check_interrupt()` instead,
  /// which throws `hsc::interrupted`. Nothing here is global: one manager,
  /// one stop state, so concurrent tasks stop independently.
  ///@{
  void set_deadline(std::optional<std::chrono::steady_clock::time_point> at) noexcept { deadline_ = at; }
  [[nodiscard]] bool has_deadline() const noexcept { return deadline_.has_value(); }
  void request_stop() noexcept { stop_ = true; }
  [[nodiscard]] bool stopping() noexcept {
    if (!stop_ && deadline_ && std::chrono::steady_clock::now() > *deadline_) stop_ = true;
    return stop_;
  }
  void mark_partial() noexcept { partial_ = true; }
  [[nodiscard]] bool partial() const noexcept { return partial_; }
  void reset_stop() noexcept {
    stop_ = false;
    partial_ = false;
  }
  void check_interrupt() {
    if (stopping()) throw interrupted("deadline reached");
  }
  /// \brief The poll of a hot loop that can run for seconds on one operation
  /// (the canonicalizer's sieve on a wide head): amortised — the clock is
  /// read once per few thousand calls — and, when stopping, it throws
  /// `interrupted`, which the nearest closure loop catches to return what it
  /// had (`diagram.cc`). One increment and a mask on the hot path.
  void poll() {
    if ((++polls_ & 0x3FFF) == 0 && stopping()) throw interrupted("deadline reached");
  }
  ///@}

 private:
  shape_table shapes_;
  op_table operations_;
  std::vector<std::unique_ptr<support_algebra>> theories_;
  std::unique_ptr<diagram_engine> diagrams_;
  case_evaluator* cases_ = nullptr;
  std::optional<std::chrono::steady_clock::time_point> deadline_;
  bool stop_ = false;
  bool partial_ = false;
  std::uint32_t polls_ = 0;
};

}  // namespace hsc::core
