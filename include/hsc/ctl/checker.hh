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
  /// The inverted event terms, asked for the first time a backward operator
  /// needs them; an empty span means unavailable (those nodes are refused).
  std::function<std::span<const core::code>()> pred_events;
  /// The same converses unprotected (no `within(R)`), for the hulls, which
  /// meet with their argument every round; optional — `pred_events` when
  /// absent.
  std::function<std::span<const core::code>()> raw_pred_events;
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

  /// Whether `R` has a cycle: `gfp(next)·R ≠ ∅` by its witness, once.
  bool has_cycles();
  /// \name For the witness tree (`hsc/trace/witness.hh`)
  ///@{
  [[nodiscard]] const model& the_model() const noexcept { return m_; }
  [[nodiscard]] forward& form() noexcept { return fw_; }
  [[nodiscard]] formulas& forms() noexcept { return f_; }
  /// The one-step term of a direction (`none` when there are no events).
  core::code step_term(bool backward);
  /// \brief The states of \p x0 with an infinite path through \p x0 in the
  /// direction asked (`backward`: `gfp(pred)·x0`, else `gfp(next)·x0`), by
  /// the round form; the frontier form (`algorithm.md` §3) under
  /// `HSC_CTL_GFP=frontier`, when the converses are available.
  core::code hull(bool backward, core::code x0);
  /// Whether that hull is nonempty: the existential `has_image` when the
  /// engine's fast cycle witness is on, else the hull itself.
  bool hull_nonempty(bool backward, core::code x0);
  /// The first `nonempty?` leaf of \p form's tree that answers yes, if any.
  std::optional<q_id> answering_leaf(const forward_form& form);
  /// `gfp(pred)·Sat f`, the states of `Sat f` with an infinite `f`-path.
  std::optional<core::code> eg_hull(node_id f);
  ///@}
  /// Is the set of \p s nonempty? Existential at the outermost operator
  /// (`algorithm.md` §6) when enabled, else by `eval`; `nullopt` when refused.
  std::optional<bool> nonempty(set_id s);
  /// The `HSC_CTL_EXIST` variation point (default on).
  static bool existential_enabled();
  /// The `HSC_CTL_OTF` variation point (default on): a goal inside a closure
  /// constrained by a temporal formula is searched breadth-first, stopping
  /// at the first hit.
  static bool otf_enabled();
  /// `HSC_CTL_TRACE=1`: one line on stderr per set expression and Sat node
  /// evaluated, with its wall time — the observation point of the checker.
  static bool trace_enabled();

 private:
  using code = core::code;
  /// The events of a direction: forward, or the inverted ones.
  [[nodiscard]] std::span<const code> events(bool backward) {
    if (!backward) return m_.next_events;
    if (!pred_) pred_ = m_.pred_events ? m_.pred_events() : std::span<const code>{};
    return *pred_;
  }
  /// The events a hull steps with: the raw converses when offered.
  [[nodiscard]] std::span<const code> hull_events(bool backward) {
    if (!backward || !m_.raw_pred_events) return events(backward);
    if (!raw_pred_) raw_pred_ = m_.raw_pred_events();
    return raw_pred_->empty() ? events(true) : *raw_pred_;
  }
  /// The one-step image (`backward`: preimage) of \p s; `none` for no
  /// events.
  code step(bool backward, code s);
  /// The same with the hull's events (their sum, built once).
  code hull_step(bool backward, code s);
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
  /// Is a state of the selector \p sel reached from the set of \p r0 through
  /// `q`-states? The on-the-fly search of `algorithm.md` §6.
  std::optional<bool> exists_through(set_id r0, node_id q, code sel);
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
  code raw_pred_step_ = core::none;  ///< the hull's backward step, built once
  bool raw_pred_built_ = false;
  std::optional<std::span<const code>> raw_pred_;
  bool next_built_ = false;
  bool pred_built_ = false;
  std::optional<std::span<const code>> pred_;  ///< the inverted events, once asked
};

}  // namespace hsc::ctl
