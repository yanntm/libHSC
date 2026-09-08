/// \file surface_translate.cc
/// \brief Translator core: form dispatch, the declarations — leaves,
/// arrays, the shape and the frontier order it fixes, the initial state.

#include <cstdint>

#include "surface_translator.hh"

namespace hsc::surface {

void translator::dispatch(const datum& form) {
  if (!form.is_list() || form.items().empty()) {
    fail(form, "a top-level form must be a non-empty list");
  }
  const std::string& kw = form.head();
  if (kw == "leaf") do_leaf(form);
  else if (kw == "array") do_array(form);
  else if (kw == "alt") do_combinator(form, /*is_alt=*/true);
  else if (kw == "seq") do_combinator(form, /*is_alt=*/false);
  else if (kw == "shape") do_shape(form);
  else if (kw == "init") do_init(form);
  else if (kw == "event") do_event(form);
  else if (kw == "family") do_family(form);
  else if (kw == "reach") do_reach(form);
  else if (kw == "apply") do_apply(form);
  else if (kw == "word") do_word(form);
  else if (kw == "get-witness") do_get_witness(form);
  else if (kw == "get-states") do_get_states(form);
  else if (kw == "states") do_states(form);
  else if (kw == "max-value") do_max_value(form);
  else if (kw == "select") do_select(form);
  else if (kw == "count") do_count(form);
  else if (kw == "leaf-weight") do_leaf_weight(form);
  else if (kw == "nodes") do_nodes(form);
  else if (kw == "print") do_print(form);
  else if (kw == "expect") do_expect(form);
  else if (kw == "ctl") do_ctl(form);
  else if (kw == "expect-ctl") do_expect_ctl(form);
  else if (kw == "gfp") do_gfp(form);
  else if (kw == "bill") do_bill(form);
  else fail(form, "unknown form '" + kw + "'");
}

void translator::do_leaf(const datum& form) {
  const std::string& name = sym(arg(form, 1, "leaf name"));
  if (leaves_.contains(name)) fail(form, "leaf '" + name + "' redeclared");
  leaf_decl d;
  if (form.items().size() > 2) {  // optional: (leaf NAME LO HI)
    d.bounded = true;
    d.lo = as_int(arg(form, 2, "lower bound"));
    d.hi = as_int(arg(form, 3, "upper bound"));
    if (d.hi <= d.lo) fail(form, "empty domain [" + std::to_string(d.lo) +
                                     "," + std::to_string(d.hi) + ")");
  }
  leaves_.emplace(name, d);
}

/// `(array NAME CELL…)`: the named leaves, in index order, are
/// addressable as NAME[i]. Cells may sit anywhere in the shape;
/// placement resolves on first use.
void translator::do_array(const datum& form) {
  const std::string& name = sym(arg(form, 1, "array name"));
  if (form.items().size() < 3) fail(form, "array needs at least one cell");
  if (arrays_.contains(name)) fail(form, "array '" + name + "' redeclared");
  array_decl a;
  for (std::size_t i = 2; i < form.items().size(); ++i) {
    const std::string& cell = sym(form.items()[i]);
    if (!leaves_.contains(cell)) {
      fail(form.items()[i], "array cell '" + cell + "' is not a leaf");
    }
    a.cells.push_back(cell);
  }
  arrays_.emplace(name, std::move(a));
}

core::shape_code translator::build_sort(const datum& d) {
  if (d.is_atom()) {
    const std::string& name = d.text();
    if (name == "unit") return mgr_.shapes().unit();
    auto it = leaves_.find(name);
    if (it == leaves_.end()) fail(d, "shape uses undeclared leaf '" + name + "'");
    if (it->second.placed) fail(d, "leaf '" + name + "' used twice in the shape");
    it->second.placed = true;
    it->second.index = order_.size();
    order_.push_back(name);
    return leaf_sort_;
  }
  const std::string& ctor = d.head();
  const auto& items = d.items();
  if (ctor == "pair") {
    if (items.size() != 3) fail(d, "pair takes exactly two sorts");
    const core::shape_code h = build_sort(items[1]);
    const core::shape_code t = build_sort(items[2]);
    return mgr_.shapes().pair(h, t);
  }
  if (ctor == "spine") return build_spine(d, 1);
  if (ctor == "balanced") {
    if (items.size() < 2) fail(d, "balanced needs at least one sort");
    return build_balanced(std::span(items).subspan(1));
  }
  fail(d, "unknown sort constructor '" + ctor + "'");
}

/// `(spine a b …)` → pair(a, pair(b, … unit)). Frontier is a, b, ….
core::shape_code translator::build_spine(const datum& d, std::size_t from) {
  if (from >= d.items().size()) return mgr_.shapes().unit();
  const core::shape_code head = build_sort(d.items()[from]);
  return mgr_.shapes().pair(head, build_spine(d, from + 1));
}

/// Split in half, left-biased. Frontier is the list order.
core::shape_code translator::build_balanced(std::span<const datum> xs) {
  if (xs.size() == 1) return build_sort(xs[0]);
  const std::size_t mid = (xs.size() + 1) / 2;
  const core::shape_code h = build_balanced(xs.subspan(0, mid));
  const core::shape_code t = build_balanced(xs.subspan(mid));
  return mgr_.shapes().pair(h, t);
}

void translator::do_shape(const datum& form) {
  if (top_ != core::none) fail(form, "shape declared twice");
  order_.clear();
  top_ = build_sort(arg(form, 1, "sort"));
  for (const auto& [name, decl] : leaves_) {
    if (!decl.placed) fail(form, "leaf '" + name + "' is not in the shape");
  }
  defaults_.assign(order_.size(), 0);
  for (std::size_t i = 0; i < order_.size(); ++i) {
    const leaf_decl& d = leaves_.at(order_[i]);
    if (d.bounded) defaults_[i] = d.lo;  // bounded default: LO; else 0
  }
  init_ = defaults_;
}

const leaf_decl& translator::require_leaf(const datum& d) {
  const std::string& name = sym(d);
  auto it = leaves_.find(name);
  if (it == leaves_.end()) fail(d, "unknown leaf '" + name + "'");
  if (top_ == core::none) fail(d, "leaf used before the shape is declared");
  return it->second;
}

void translator::do_init(const datum& form) {
  if (top_ == core::none) fail(form, "init before shape");
  if (form.items().size() == 1) return;  // (init): the base word as is
  const bool pairs =
      form.items().size() > 1 && form.items()[1].is_list() &&
      form.items()[1].items().size() == 2 &&
      form.items()[1].items()[0].is_atom() &&
      leaves_.contains(form.items()[1].items()[0].text());
  if (pairs) {
    if (seed_override_) fail(form, "pair init after an init event");
    for (std::size_t i = 1; i < form.items().size(); ++i) {
      const datum& pair = form.items()[i];
      if (!pair.is_list() || pair.items().size() != 2) {
        fail(pair, "init entry must be (leaf value)");
      }
      const leaf_decl& d = require_leaf(pair.items()[0]);
      init_[d.index] = as_int(pair.items()[1]);
    }
    return;
  }
  if (form.items().size() != 2) fail(form, "init takes pairs or one event term");
  if (seed_override_) fail(form, "init event already set");
  const code seeded =
      mgr_.diagrams().apply_local(read_evterm(form.items()[1]), initial());
  if (seeded == core::none) {
    fail(form, "init event produced no state: the initial set is empty, "
               "the model is malformed");
  }
  seed_override_ = seeded;
}

code translator::build_point(core::shape_code sort, std::size_t& next,
                 const std::vector<std::int32_t>& values) {
  core::diagram_engine& diagrams = mgr_.diagrams();
  switch (mgr_.shapes().kind(sort)) {
    case core::shape_kind::unit:
      return diagrams.one();
    case core::shape_kind::leaf:
      return theory_->singleton(values[next++]);
    case core::shape_kind::pair: {
      const code head = build_point(mgr_.shapes().head(sort), next, values);
      const code tail = build_point(mgr_.shapes().tail(sort), next, values);
      return diagrams.rectangle(sort, head, tail);
    }
  }
  return core::none;
}

/// The base word: `init_` (declared LO defaults, edited by pair inits).
code translator::initial() {
  std::size_t next = 0;
  return build_point(top_, next, init_);
}

}  // namespace hsc::surface
