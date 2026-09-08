/// \file checker.hh
/// \brief Evaluation of a forward form over a model: set expressions and
/// backward `Sat`, memoised per id; the verdict. See `algorithm.md` §3–§7.
///
/// The model is abstract: a sort, the reachable set, the seed, the forward
/// event terms, the inverted ones when the caller has them, a selector term
/// per state formula, and the reachable deadlocks as data. Nothing here
/// knows how any of those were built. Every question is asked in the
/// calculus's own algebra — `join / meet / minus`, term application, the
/// saturated `lfp` over filtered events, `gfp` — and a question the model
/// cannot answer (a backward operator without inverted events) is refused,
/// never approximated.
#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "hsc/core/code.hh"
#include "hsc/core/shape.hh"
#include "hsc/ctl/forward.hh"

namespace hsc::core {
class manager;
}

namespace hsc::ctl {

/// What the checker evaluates against.
struct model {
  core::shape_code sort = core::none;
  core::code reach = core::none;  ///< `R`
  core::code init = core::none;   ///< the seed, `I ⊆ R`
  std::vector<core::code> next_events;  ///< forward event terms at `sort`
  std::vector<core::code> pred_events;  ///< inverted terms; empty: unavailable
  /// The selector term of a state formula (a guard-only event at `sort`):
  /// applied to a set it keeps the states where the formula holds. Never
  /// asked for a constant.
  std::function<core::code(node_id)> selector;
  core::code dead = core::none;  ///< the reachable deadlocks, `none` if impossible
};

enum class verdict : std::uint8_t { yes, no, unknown };
[[nodiscard]] const char* name(verdict v) noexcept;

class checker {
 public:
  checker(core::manager& mgr, const model& m, forward& fw);

  /// The verdict of a converted property at the seed.
  verdict check(const forward_form& form);

  /// The set denoted by a set expression; `nullopt` when refused.
  std::optional<core::code> eval(set_id s);
  /// `Sat(φ) ⊆ R`; `nullopt` when refused (a backward operator without
  /// inverted events).
  std::optional<core::code> sat(node_id f);

  /// Whether `R` has a cycle: `gfp(next)·R ≠ ∅`, computed once.
  bool has_cycles();

 private:
  using code = core::code;
  /// The events of a direction: forward, or the inverted ones.
  [[nodiscard]] std::span<const code> events(bool backward) const {
    return backward ? m_.pred_events : m_.next_events;
  }
  /// The one-step image (`backward`: preimage) of \p s; `none` for no
  /// events.
  code step(bool backward, code s);
  /// The saturated closure of the direction's events each composed with
  /// \p sel: for `before == true` the filter applies before the step
  /// (`t ∘ sel`, forward `fwdu`), else after it (`sel ∘ p`, backward
  /// `EU`). `sel == id` is the plain closure. Memoised per (direction,
  /// before, sel).
  code closure(bool backward, code sel, bool before);
  /// `lfp` from \p from over the direction's events constrained by the
  /// formula \p q: a state formula composes its selector into the events;
  /// any other formula meets its `Sat` around each step. `before` as in
  /// `closure`.
  std::optional<code> constrained_lfp(bool backward, code from, node_id q,
                                      bool before);
  /// The seed of a constrained closure: \p s restricted to \p q — the
  /// selector for a state formula, the `Sat` as data otherwise.
  std::optional<code> restrict(code s, node_id q);
  /// The subset of \p s where the state formula \p f holds.
  code filter(code s, node_id f);
  verdict ask(q_id q);
  std::optional<code> sat_eu(node_id f, node_id g);
  std::optional<code> sat_eg(node_id f);

  core::manager& mgr_;
  const model& m_;
  forward& fw_;
  formulas& f_;
  std::unordered_map<set_id, std::optional<code>> set_memo_;
  std::unordered_map<node_id, std::optional<code>> sat_memo_;
  std::unordered_map<node_id, code> sel_memo_;
  struct closure_key {
    bool backward;
    bool before;
    code sel;
    friend bool operator==(const closure_key&, const closure_key&) = default;
  };
  struct closure_hash {
    std::size_t operator()(const closure_key& k) const noexcept;
  };
  std::unordered_map<closure_key, code, closure_hash> closure_memo_;
  std::optional<bool> cycles_;
  code next_step_ = core::none;  ///< the one-step forward term, built once
  code pred_step_ = core::none;
  bool steps_built_ = false;
};

}  // namespace hsc::ctl
