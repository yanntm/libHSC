/// \file fsp_ground.cc
/// \brief M2LTS for FSP: evaluate one process to its ground LTS
/// (`hsc/fsp/algorithm.md` §2).

#include <deque>

#include "hsc/fsp/ground.hh"

namespace hsc::fsp {
namespace {

using env_t = std::map<std::string, long>;

// -- expression evaluation --------------------------------------------------

long eval(const expr& e, const env_t& env, int line) {
  switch (e.k) {
    case expr::kind::integer:
      return e.value;
    case expr::kind::name: {
      const auto it = env.find(e.name);
      if (it == env.end())
        throw ground_error(line, "unknown name '" + e.name + "'");
      return it->second;
    }
    case expr::kind::unary: {
      const long a = eval(e.args[0], env, line);
      if (e.op == "-") return -a;
      if (e.op == "!") return a == 0 ? 1 : 0;
      throw ground_error(line, "unknown unary operator '" + e.op + "'");
    }
    case expr::kind::binary: {
      const long a = eval(e.args[0], env, line);
      // Short-circuit keeps && / || total on partial guards.
      if (e.op == "&&") return (a != 0 && eval(e.args[1], env, line) != 0);
      if (e.op == "||") return (a != 0 || eval(e.args[1], env, line) != 0);
      const long b = eval(e.args[1], env, line);
      if (e.op == "+") return a + b;
      if (e.op == "-") return a - b;
      if (e.op == "*") return a * b;
      if (e.op == "/" || e.op == "%") {
        if (b == 0) throw ground_error(line, "division by zero");
        return e.op == "/" ? a / b : a % b;
      }
      if (e.op == "<<") return a << b;
      if (e.op == ">>") return a >> b;
      if (e.op == "==") return a == b;
      if (e.op == "!=") return a != b;
      if (e.op == "<") return a < b;
      if (e.op == "<=") return a <= b;
      if (e.op == ">") return a > b;
      if (e.op == ">=") return a >= b;
      throw ground_error(line, "unknown operator '" + e.op + "'");
    }
  }
  throw ground_error(line, "malformed expression");
}

// -- the grounding machine --------------------------------------------------

class grounder {
 public:
  grounder(const model& m, const process& p) : p_(p) {
    for (const auto& [n, e] : m.consts) consts_[n] = eval(e, consts_, p.line);
    for (const auto& [n, r] : m.ranges) ranges_[n] = bounds(r, consts_, p.line);
  }

  ground_lts run() {
    out_.name = p_.name;
    out_.is_property = p_.is_property;
    seed();
    while (!queue_.empty()) {
      const int s = queue_.front();
      queue_.pop_front();
      const auto [def, args] = meaning_[s];
      env_t env = consts_;
      const state_def& d = p_.states[def];
      for (std::size_t i = 0; i < d.params.size(); ++i)
        env[d.params[i].first] = args[i];
      expand_body(s, d.body, env);
    }
    finish_alphabet_and_hiding();
    return std::move(out_);
  }

 private:
  const process& p_;
  env_t consts_;
  std::map<std::string, std::pair<long, long>> ranges_;
  ground_lts out_;
  /// Named ground states: (statedef index, argument tuple) → state number.
  std::map<std::pair<int, std::vector<long>>, int> ids_;
  /// state number → its (statedef, args); anonymous states carry (-1, {}).
  std::vector<std::pair<int, std::vector<long>>> meaning_;
  std::deque<int> queue_;

  std::pair<long, long> bounds(const rng& r, const env_t& env, int line) {
    if (!r.name.empty()) {
      const auto it = ranges_.find(r.name);
      if (it == ranges_.end())
        throw ground_error(line, "unknown range '" + r.name + "'");
      return it->second;
    }
    return {eval(*r.lo, env, line), eval(*r.hi, env, line)};
  }

  int def_index(const std::string& name, int line) const {
    for (std::size_t i = 0; i < p_.states.size(); ++i)
      if (p_.states[i].name == name) return static_cast<int>(i);
    throw ground_error(line, "unknown state '" + name + "' in process '" +
                                 p_.name + "'");
  }

  /// Are \p args within the declared parameter ranges of statedef \p def?
  /// (LTSA maps an out-of-range target to ERROR; the caller decides.)
  bool in_bounds(int def, const std::vector<long>& args, int line) {
    const state_def& d = p_.states[def];
    env_t env = consts_;
    for (std::size_t i = 0; i < args.size(); ++i) {
      const auto [lo, hi] = bounds(d.params[i].second, env, line);
      if (args[i] < lo || args[i] > hi) return false;
      env[d.params[i].first] = args[i];
    }
    return true;
  }

  int intern(int def, std::vector<long> args, int line) {
    const state_def& d = p_.states[def];
    if (args.size() != d.params.size())
      throw ground_error(line, "state '" + d.name + "' takes " +
                                   std::to_string(d.params.size()) +
                                   " arguments");
    if (!in_bounds(def, args, line))
      throw ground_error(line, "argument of state '" + d.name +
                                   "' outside its declared range");
    const auto [it, fresh] = ids_.try_emplace({def, args}, -1);
    if (fresh) {
      it->second = new_state(pretty(d.name, args));
      meaning_[it->second] = {def, std::move(args)};
      queue_.push_back(it->second);
    }
    return it->second;
  }

  int new_state(std::string name) {
    out_.state_names.push_back(std::move(name));
    meaning_.emplace_back(-1, std::vector<long>{});
    return static_cast<int>(out_.state_names.size()) - 1;
  }

  static std::string pretty(const std::string& base,
                            const std::vector<long>& args) {
    std::string s = base;
    for (const long a : args) s += "." + std::to_string(a);
    return s;
  }

  void seed() {
    if (p_.init_ref) {
      const state_ref& r = *p_.init_ref;
      if (r.is_error)
        throw ground_error(r.line, "a process cannot start in ERROR");
      std::vector<long> args;
      for (const expr& a : r.args) args.push_back(eval(a, consts_, r.line));
      intern(def_index(r.name, r.line), std::move(args), r.line);
    } else {
      // Inline anonymous body: a synthetic initial state expanded directly
      // (never queued — named states it references are queued by intern()).
      const int s = new_state(p_.name + ".0");
      expand_body(s, p_.init_body, consts_);
    }
  }

  void expand_body(int src, const std::vector<branch>& body,
                   const env_t& env) {
    for (const branch& b : body) {
      if (b.guard && eval(*b.guard, env, b.line) == 0) continue;
      expand_chain(src, b, 0, env);
    }
  }

  /// Walk the prefix chain: expand label \p pos under \p env; each ground
  /// choice yields an edge — to the resolved target on the last label, to
  /// a fresh anonymous state (then recurse) otherwise.
  void expand_chain(int src, const branch& b, std::size_t pos,
                    const env_t& env) {
    std::vector<std::pair<glabel, env_t>> grounds;
    expand_pattern(b.labels[pos], 0, {}, env, grounds);
    const bool last = pos + 1 == b.labels.size();
    for (auto& [atoms, env2] : grounds) {
      int dst;
      if (last) {
        dst = resolve_target(b.target, env2);
      } else {
        dst = new_state(out_.state_names[src] + "@" + std::to_string(pos + 1));
        // Anonymous states are expanded inline, not queued: their only
        // content is the rest of this chain under this valuation.
      }
      out_.rel[atoms].emplace_back(src, dst);
      if (!last) expand_chain(dst, b, pos + 1, env2);
    }
  }

  int resolve_target(const state_ref& r, const env_t& env) {
    if (r.is_error) return -1;
    for (std::size_t i = 0; i < p_.states.size(); ++i)
      if (p_.states[i].name == r.name) {
        std::vector<long> args;
        for (const expr& a : r.args) args.push_back(eval(a, env, r.line));
        // LTSA convention: a target argument outside its declared range
        // is ERROR (never_fill_table's counter errs exactly this way).
        if (args.size() == p_.states[i].params.size() &&
            !in_bounds(static_cast<int>(i), args, r.line))
          return -1;
        return intern(static_cast<int>(i), std::move(args), r.line);
      }
    // The process's own name refers to its initial state (state 0).
    if (r.name == p_.name && r.args.empty()) return 0;
    throw ground_error(r.line, "unknown state '" + r.name + "' in process '" +
                                   p_.name + "'");
  }

  /// Expand one pattern element-by-element: sets and anonymous ranges
  /// choose, binders choose and bind for the rest of the branch.
  void expand_pattern(const label_pattern& p, std::size_t i, glabel acc,
                      const env_t& env,
                      std::vector<std::pair<glabel, env_t>>& out) {
    if (i == p.elems.size()) {
      out.emplace_back(std::move(acc), env);
      return;
    }
    const label_elem& el = p.elems[i];
    switch (el.k) {
      case label_elem::kind::name: {
        acc.push_back(el.text);
        expand_pattern(p, i + 1, std::move(acc), env, out);
        return;
      }
      case label_elem::kind::index: {
        acc.push_back(std::to_string(eval(*el.ix, env, p.line)));
        expand_pattern(p, i + 1, std::move(acc), env, out);
        return;
      }
      case label_elem::kind::choice: {
        const auto [lo, hi] = bounds(el.range, env, p.line);
        for (long v = lo; v <= hi; ++v) {
          glabel a = acc;
          a.push_back(std::to_string(v));
          expand_pattern(p, i + 1, std::move(a), env, out);
        }
        return;
      }
      case label_elem::kind::binder: {
        const auto [lo, hi] = bounds(el.range, env, p.line);
        for (long v = lo; v <= hi; ++v) {
          glabel a = acc;
          a.push_back(std::to_string(v));
          env_t e2 = env;
          e2[el.var] = v;
          expand_pattern(p, i + 1, std::move(a), e2, out);
        }
        return;
      }
      case label_elem::kind::set: {
        for (const label_pattern& alt : el.alts) {
          // Expand the sub-pattern fully, then resume the outer pattern
          // with each result's atoms appended and bindings kept.
          std::vector<std::pair<glabel, env_t>> subs;
          expand_pattern(alt, 0, {}, env, subs);
          for (auto& [sub_atoms, sub_env] : subs) {
            glabel a = acc;
            a.insert(a.end(), sub_atoms.begin(), sub_atoms.end());
            expand_pattern(p, i + 1, std::move(a), sub_env, out);
          }
        }
        return;
      }
    }
  }

  void finish_alphabet_and_hiding() {
    for (const auto& [l, _] : out_.rel) out_.alphabet.insert(l);
    for (const label_pattern& pat : p_.extension) {
      std::vector<std::pair<glabel, env_t>> grounds;
      expand_pattern(pat, 0, {}, consts_, grounds);
      for (auto& [atoms, _] : grounds) out_.alphabet.insert(atoms);
    }
    // Hiding: prefix match on the atom sequence; matched labels' edges
    // become tau, and the label leaves relation and alphabet.
    std::vector<glabel> prefixes;
    for (const label_pattern& pat : p_.hidden) {
      std::vector<std::pair<glabel, env_t>> grounds;
      expand_pattern(pat, 0, {}, consts_, grounds);
      for (auto& [atoms, _] : grounds) prefixes.push_back(atoms);
    }
    if (prefixes.empty()) return;
    const auto hidden = [&](const glabel& l) {
      for (const glabel& p : prefixes)
        if (p.size() <= l.size() && std::equal(p.begin(), p.end(), l.begin()))
          return true;
      return false;
    };
    for (auto it = out_.rel.begin(); it != out_.rel.end();) {
      if (hidden(it->first)) {
        out_.tau.insert(out_.tau.end(), it->second.begin(), it->second.end());
        out_.alphabet.erase(it->first);
        it = out_.rel.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = out_.alphabet.begin(); it != out_.alphabet.end();)
      it = hidden(*it) ? out_.alphabet.erase(it) : std::next(it);
  }
};

}  // namespace

ground_lts ground(const model& m, const process& p) {
  return grounder(m, p).run();
}

std::string to_string(const glabel& l) {
  std::string s;
  for (std::size_t i = 0; i < l.size(); ++i) s += (i ? "." : "") + l[i];
  return s;
}

}  // namespace hsc::fsp
