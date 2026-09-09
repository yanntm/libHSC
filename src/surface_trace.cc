/// \file surface_trace.cc
/// \brief The path commands: `(path NAME FROM TO [through ATOM])` and
/// `(expect-path NAME K)`.
///
/// A path is a shortest run of the default system from a state of FROM to a
/// state of TO whose intermediate states satisfy the `through` atom, found
/// symbolically (`hsc/trace/`): printed as alternating word literals and
/// event names, runnable by the explicit engine, and bound under NAME for
/// `expect-path` (its length) and `get-witness` (its last state).
#include "hsc/core/operation.hh"
#include "hsc/trace/path.hh"
#include "surface_translator.hh"

namespace hsc::surface {

/// `(path NAME FROM TO [through ATOM])`.
void translator::do_path(const datum& form) {
  if (top_ == core::none) fail(form, "path before shape");
  const std::string& name = sym(arg(form, 1, "path name"));
  const code from = named(arg(form, 2, "source result"));
  const code to = named(arg(form, 3, "target result"));
  code constraint = core::op_table::id;
  if (form.items().size() > 4) {
    const datum& kw = form.items()[4];
    if (!kw.is_atom() || kw.text() != "through" || form.items().size() != 6) {
      fail(form, "path takes FROM TO [through ATOM]");
    }
    constraint = read_evterm(
        datum::list({atom_datum("when", form), form.items()[5]}, form.line()));
  }
  // The inverted events, raw: exact on R, and applied to one state at a time.
  std::vector<code> preds;
  try {
    core::inverter inv(mgr_);
    const code reach = run_reach(false);
    for (const code ev : events_) preds.push_back(inv(top_, ev, reach));
  } catch (const unsupported_error&) {
    preds.clear();  // the search falls back to forward tests
  }
  trace::graph g;
  g.sort = top_;
  g.events = events_;
  g.preds = preds;
  g.within = run_reach(false);
  g.one_state = [this](code set) -> code {
    std::vector<std::int32_t> values;
    if (!first_word(top_, set, values)) return core::none;
    std::size_t next = 0;
    return build_point(top_, next, values);
  };
  const std::optional<trace::path_result> p = trace::path(mgr_, g, from, to, constraint);
  if (!p) {
    out_ << name << " path none\n";
    paths_.erase(name);
    return;
  }
  out_ << name << " path " << p->events.size() << '\n';
  for (std::size_t i = 0; i < p->states.size(); ++i) {
    std::vector<std::int32_t> values;
    first_word(top_, p->states[i], values);
    out_ << "  ";
    print_word(values);
    out_ << '\n';
    if (i < p->events.size()) out_ << "  " << event_names_[p->events[i]] << '\n';
  }
  paths_[name] = p->events.size();
  results_[name] = p->states.back();
}

/// `(expect-path NAME K)`: the path exists and has K steps; `none` for no path.
void translator::do_expect_path(const datum& form) {
  const std::string& name = sym(arg(form, 1, "path name"));
  const datum& want = arg(form, 2, "length or none");
  const auto it = paths_.find(name);
  const std::string got = it == paths_.end() ? "none" : std::to_string(it->second);
  const std::string w = want.is_atom() ? want.text() : "";
  if (got == w) {
    out_ << "ok " << name << " path " << w << '\n';
  } else {
    out_ << "FAIL " << name << " expected path " << w << " got " << got << '\n';
    ++failures_;
  }
}

}  // namespace hsc::surface
