/// \file fsp_to_surface.cc
/// \brief M2M for FSP: compose the ground LTSs into separable-fragment
/// surface forms, and print the model / driver files
/// (`hsc/fsp/algorithm.md` §3–4).

#include <algorithm>
#include <ostream>
#include <set>

#include "hsc/fsp/to_surface.hh"

namespace hsc::fsp {
namespace {

surface::datum atom(std::string s) {
  return surface::datum::atom(std::move(s), 0);
}
surface::datum num(long v) { return atom(std::to_string(v)); }
surface::datum list(std::vector<surface::datum> v) {
  return surface::datum::list(std::move(v), 0);
}

std::string leaf_name(const std::string& proc) {
  return proc.rfind("TASK_", 0) == 0 ? proc.substr(5) : proc;
}

std::string event_base(const glabel& l) {
  std::string s;
  for (std::size_t i = 0; i < l.size(); ++i) s += (i ? "_" : "") + l[i];
  return s;
}

/// One target piece of a participant's relation on a label: the guard's
/// source set and, unless it is the identity piece, the constant written.
struct piece {
  std::vector<int> srcs;
  int dst = 0;
  bool writes = false;
};

/// Partition \p p's relation on \p a into target pieces (`algorithm.md`
/// §3). \p error_value ≥ 0 marks the property: the relation is totalized
/// (undefined states write E, E is absorbing) and must be deterministic.
std::vector<piece> pieces_of(const ground_lts& p, const glabel& a,
                             int error_value, bool pinned) {
  const int n = static_cast<int>(p.state_names.size());
  std::map<int, std::set<int>> by_dst;  // non-self targets → sources
  std::set<int> self, covered;
  if (const auto it = p.rel.find(a); it != p.rel.end()) {
    for (const auto& [s, d] : it->second) {
      if (error_value < 0 && d < 0)
        throw transform_error("process '" + p.name +
                              "' reaches ERROR on label " + to_string(a) +
                              "' — only the property may");
      if (error_value >= 0 && !covered.insert(s).second)
        throw transform_error("property '" + p.name +
                              "' is nondeterministic on label " +
                              to_string(a));
      const int dd = d < 0 ? error_value : d;
      if (dd == s)
        self.insert(s);
      else
        by_dst[dd].insert(s);
    }
  }
  if (error_value >= 0) {
    // Totalize: states with no transition err; E itself is absorbing.
    for (int s = 0; s < n; ++s)
      if (!covered.count(s)) by_dst[error_value].insert(s);
    self.insert(error_value);
  }
  std::vector<piece> out;
  for (const auto& [d, srcs] : by_dst) {
    if (pinned)  // one piece per source: every write guarded by (== P s)
      for (const int s : srcs) out.push_back({{s}, d, true});
    else
      out.push_back({{srcs.begin(), srcs.end()}, d, true});
  }
  // Identity pieces too: a set guard would hotbit into an or across bit
  // leaves — a crossing atom the cegar bridge refuses.
  if (pinned)
    for (const int s : self) out.push_back({{s}, 0, false});
  else if (!self.empty())
    out.push_back({{self.begin(), self.end()}, 0, false});
  return out;
}

surface::datum guard_atom(const std::string& leaf, const std::vector<int>& s) {
  if (s.size() == 1) return list({atom("=="), atom(leaf), num(s[0])});
  std::vector<surface::datum> v{atom("in"), atom(leaf)};
  for (const int x : s) v.push_back(num(x));
  return list(std::move(v));
}

surface::datum write_act(const std::string& leaf, int dst) {
  return list({atom(":="), atom(leaf), num(dst)});
}

class composer {
 public:
  composer(const std::vector<ground_lts>& procs, std::size_t map_limit,
           bool pinned)
      : map_limit_(map_limit), pinned_(pinned) {
    for (const ground_lts& p : procs)
      (p.is_property ? props_ : leaves_).push_back(&p);
    if (props_.size() != 1)
      throw transform_error("expected exactly one property process, got " +
                            std::to_string(props_.size()));
    for (const ground_lts* p : leaves_) {
      const std::string n = leaf_name(p->name);
      if (n == "mon" || !names_.insert(n).second)
        throw transform_error("leaf name collision on '" + n + "'");
      leaf_names_.push_back(n);
    }
  }

  translation run() {
    const ground_lts& prop = *props_.front();
    t_.monitor_error = static_cast<int>(prop.state_names.size());
    declare();
    // Every ground label of the composition, in sorted (deterministic)
    // order; tau events per process afterwards.
    std::set<glabel> all;
    for (const ground_lts* p : leaves_)
      all.insert(p->alphabet.begin(), p->alphabet.end());
    for (const glabel& a : all) emit_label(a);
    t_.labels = all.size();
    // A property-only label is unproducible; totalization would still
    // move `mon`, so it must not become an event at all.
    for (const glabel& a : prop.alphabet)
      if (!all.count(a)) ++t_.dead_labels;
    for (std::size_t i = 0; i < leaves_.size(); ++i) emit_tau(i);
    return std::move(t_);
  }

 private:
  std::size_t map_limit_;
  bool pinned_ = false;
  std::vector<const ground_lts*> leaves_, props_;
  std::vector<std::string> leaf_names_;
  std::set<std::string> names_;
  std::set<std::string> event_names_;
  translation t_;

  void declare() {
    const ground_lts& prop = *props_.front();
    for (std::size_t i = 0; i < leaves_.size(); ++i) {
      const std::size_t n = leaves_[i]->state_names.size();
      t_.header.push_back("leaf " + leaf_names_[i] + " = " +
                          leaves_[i]->name + ", " + std::to_string(n) +
                          " states" + map_of(*leaves_[i]));
      t_.forms.push_back(list({atom("leaf"), atom(leaf_names_[i]), num(0),
                               num(static_cast<long>(n))}));
    }
    t_.header.push_back("leaf mon = property " + prop.name + ", " +
                        std::to_string(prop.state_names.size()) +
                        " states + error " +
                        std::to_string(t_.monitor_error) + map_of(prop));
    t_.forms.push_back(list({atom("leaf"), atom("mon"), num(0),
                             num(t_.monitor_error + 1)}));
    std::vector<surface::datum> spine{atom("spine")};
    for (const std::string& n : leaf_names_) spine.push_back(atom(n));
    spine.push_back(atom("mon"));
    t_.forms.push_back(list({atom("shape"), list(std::move(spine))}));
    t_.forms.push_back(list({atom("init")}));
  }

  std::string map_of(const ground_lts& p) const {
    if (p.state_names.size() > map_limit_) return " (map suppressed)";
    std::string s = ":";
    for (std::size_t i = 0; i < p.state_names.size(); ++i)
      s += " " + std::to_string(i) + "=" + p.state_names[i];
    return s;
  }

  std::string unique_name(const std::string& base) {
    std::string n = base;
    for (int k = 2; !event_names_.insert(n).second; ++k)
      n = base + "_x" + std::to_string(k);
    return n;
  }

  void emit_label(const glabel& a) {
    const ground_lts& prop = *props_.front();
    std::vector<std::size_t> parts;  // leaf indices participating
    for (std::size_t i = 0; i < leaves_.size(); ++i)
      if (leaves_[i]->alphabet.count(a)) parts.push_back(i);
    // Dead if any participant never fires it (extension-only alphabet).
    for (const std::size_t i : parts)
      if (!leaves_[i]->rel.count(a)) {
        ++t_.dead_labels;
        return;
      }
    std::vector<std::vector<piece>> pp;
    std::vector<std::string> pnames;
    for (const std::size_t i : parts) {
      pp.push_back(pieces_of(*leaves_[i], a, -1, pinned_));
      pnames.push_back(leaf_names_[i]);
    }
    if (prop.alphabet.count(a)) {
      pp.push_back(pieces_of(prop, a, t_.monitor_error, false));
      pnames.push_back("mon");
    }
    // The product of the piece sets, one event per tuple.
    std::vector<std::size_t> ix(pp.size(), 0);
    std::vector<surface::datum> pending;
    for (;;) {
      bool any_write = false;
      for (std::size_t i = 0; i < pp.size(); ++i)
        any_write |= pp[i][ix[i]].writes;
      if (!any_write) {
        ++t_.stutters;
      } else {
        std::vector<surface::datum> when{atom("when")}, act{atom("do")};
        for (std::size_t i = 0; i < pp.size(); ++i) {
          const piece& pc = pp[i][ix[i]];
          when.push_back(guard_atom(pnames[i], pc.srcs));
          if (pc.writes) act.push_back(write_act(pnames[i], pc.dst));
        }
        pending.push_back(list({atom("event"), atom(""),  // name patched below
                                list(std::move(when)), list(std::move(act))}));
      }
      std::size_t i = 0;
      for (; i < pp.size(); ++i) {
        if (++ix[i] < pp[i].size()) break;
        ix[i] = 0;
      }
      if (i == pp.size()) break;
    }
    name_and_emit(event_base(a), std::move(pending));
  }

  void emit_tau(std::size_t i) {
    const ground_lts& p = *leaves_[i];
    if (p.tau.empty()) return;
    std::map<int, std::set<int>> by_dst;
    for (const auto& [s, d] : p.tau) {
      if (d < 0)
        throw transform_error("process '" + p.name +
                              "' reaches ERROR on a hidden label");
      if (d == s)
        ++t_.stutters;
      else
        by_dst[d].insert(s);
    }
    std::vector<surface::datum> pending;
    for (const auto& [d, srcs] : by_dst) {
      // Under --pinned, tau writes split per source like any write piece.
      std::vector<std::vector<int>> guards;
      if (pinned_)
        for (const int s : srcs) guards.push_back({s});
      else
        guards.push_back({srcs.begin(), srcs.end()});
      for (auto& g : guards)
        pending.push_back(
            list({atom("event"), atom(""),
                  list({atom("when"), guard_atom(leaf_names_[i], g)}),
                  list({atom("do"), write_act(leaf_names_[i], d)})}));
    }
    name_and_emit(leaf_names_[i] + "_tau", std::move(pending));
  }

  /// Patch names into the pending events (`base`, or `base__k` when there
  /// are several) and append them to the forms.
  void name_and_emit(const std::string& base,
                     std::vector<surface::datum> pending) {
    for (std::size_t k = 0; k < pending.size(); ++k) {
      const std::string n = unique_name(
          pending.size() == 1 ? base : base + "__" + std::to_string(k + 1));
      std::vector<surface::datum> items = pending[k].items();
      items[1] = atom(n);
      t_.forms.push_back(list(std::move(items)));
      ++t_.events;
    }
  }
};

}  // namespace

translation to_surface(const std::vector<ground_lts>& procs,
                       std::size_t map_limit, bool pinned) {
  return composer(procs, map_limit, pinned).run();
}

void print_model(std::ostream& os, const translation& t) {
  for (const std::string& h : t.header) os << "; " << h << '\n';
  for (const surface::datum& d : t.forms) {
    surface::write(os, d);
    os << '\n';
  }
}

void print_driver(std::ostream& os, const translation& t,
                  const std::string& model_file) {
  os << "; Driver for " << model_file
     << " — a CAC08 subject: can the property DFA err?\n"
     << "; Expectations are pinned by the campaign after a verified run.\n"
     << "(input " << model_file << ")\n"
     << "(cegar v (== mon " << t.monitor_error << "))\n"
     << "(xreach x)\n";
}

}  // namespace hsc::fsp
