/// \file surface_walk.hh
/// \brief Internal: map a function over every expression position of the
/// spec's forms — `when` items, action targets and right-hand sides,
/// `select` atoms, inline event terms, init events. For passes that
/// rewrite expressions without restructuring clauses; heavier passes
/// (dropping actions, moving bits) keep their own walks.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "hsc/surface/sexpr.hh"

namespace hsc::surface {

using expr_fn = std::function<datum(const datum&)>;

namespace walk_detail {

inline datum action(const datum& a, const expr_fn& f) {
  if (!a.is_list() || a.items().size() < 2) return a;
  std::vector<datum> kids{a.items()[0]};
  kids.push_back(f(a.items()[1]));  // a target is an expression position too
  for (std::size_t i = 2; i < a.items().size(); ++i) {
    kids.push_back(a.head() == "havoc" ? a.items()[i] : f(a.items()[i]));
  }
  return datum::list(std::move(kids), a.line());
}

inline datum clause(const datum& c, const expr_fn& f) {
  if (!c.is_list() || c.items().empty()) return c;
  std::vector<datum> kids{c.items()[0]};
  if (c.head() == "when") {
    for (std::size_t i = 1; i < c.items().size(); ++i) {
      kids.push_back(f(c.items()[i]));
    }
  } else if (c.head() == "do") {
    for (std::size_t i = 1; i < c.items().size(); ++i) {
      kids.push_back(action(c.items()[i], f));
    }
  } else {
    return c;
  }
  return datum::list(std::move(kids), c.line());
}

inline datum evterm(const datum& d, const expr_fn& f) {
  if (!d.is_list() || d.items().empty()) return d;
  const std::string& h = d.head();
  if (h == "when" || h == "do") return clause(d, f);
  if (h == "alt" || h == "seq") {
    std::vector<datum> kids{d.items()[0]};
    for (std::size_t i = 1; i < d.items().size(); ++i) {
      kids.push_back(evterm(d.items()[i], f));
    }
    return datum::list(std::move(kids), d.line());
  }
  return d;
}

}  // namespace walk_detail

/// One top-level form, every expression position mapped through \p f.
inline datum map_exprs(const datum& form, const expr_fn& f) {
  using namespace walk_detail;
  if (!form.is_list() || form.items().empty()) return form;
  const std::string& h = form.head();
  const int line = form.line();
  if (h == "event" || h == "family") {
    const std::size_t body = h == "event" ? 2 : 3;
    std::vector<datum> kids(form.items().begin(),
                            form.items().begin() +
                                static_cast<std::ptrdiff_t>(body));
    for (std::size_t i = body; i < form.items().size(); ++i) {
      kids.push_back(clause(form.items()[i], f));
    }
    return datum::list(std::move(kids), line);
  }
  if (h == "alt" || h == "seq") {
    std::vector<datum> kids(form.items().begin(), form.items().begin() + 2);
    for (std::size_t i = 2; i < form.items().size(); ++i) {
      kids.push_back(evterm(form.items()[i], f));
    }
    return datum::list(std::move(kids), line);
  }
  if (h == "select") {
    std::vector<datum> kids(form.items().begin(), form.items().begin() + 3);
    for (std::size_t i = 3; i < form.items().size(); ++i) {
      kids.push_back(f(form.items()[i]));
    }
    return datum::list(std::move(kids), line);
  }
  if (h == "reach" || h == "apply") {
    std::vector<datum> kids{form.items()[0]};
    for (std::size_t i = 1; i < form.items().size(); ++i) {
      kids.push_back(form.items()[i].is_list() ? evterm(form.items()[i], f)
                                               : form.items()[i]);
    }
    return datum::list(std::move(kids), line);
  }
  if (h == "init" && form.items().size() == 2 && form.items()[1].is_list()) {
    return datum::list({form.items()[0], evterm(form.items()[1], f)}, line);
  }
  return form;
}

}  // namespace hsc::surface
