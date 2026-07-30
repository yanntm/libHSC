/// \file cegar/model.cc — leaves, events, monitor, shape; .cts I/O;
/// the monolithic oracle walk and concrete replay.

#include "hsc/cegar/model.hh"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <unordered_map>

#include "hsc/util/hash.hh"

namespace hsc::cegar {

std::optional<std::int32_t> lts::fire(const word& w) const {
  std::int32_t q = 0;
  for (letter a : w) {
    q = step(q, a);
    if (q < 0) return std::nullopt;
  }
  return q;
}

std::string lts::serialize() const {
  std::string out;
  out.reserve(8 + delta.size() * 4);
  auto put = [&out](std::int32_t v) {
    char b[4];
    std::memcpy(b, &v, 4);
    out.append(b, 4);
  };
  put(n_states);
  put(n_letters);
  for (std::int32_t d : delta) put(d);
  return out;
}

std::optional<letter> event::letter_for(std::int32_t leaf) const {
  for (const auto& [l, a] : support)
    if (l == leaf) return a;
  return std::nullopt;
}

shape shape::comb(std::int32_t n) {
  shape s;
  if (n <= 0) return s;
  std::int32_t cur = static_cast<std::int32_t>(s.nodes.size());
  s.nodes.push_back({0, -1, -1});
  for (std::int32_t i = 1; i < n; ++i) {
    std::int32_t leaf_node = static_cast<std::int32_t>(s.nodes.size());
    s.nodes.push_back({i, -1, -1});
    std::int32_t cut = static_cast<std::int32_t>(s.nodes.size());
    s.nodes.push_back({-1, cur, leaf_node});
    cur = cut;
  }
  s.root = cur;
  return s;
}

word model::project(const eword& w, std::int32_t leaf) const {
  word out;
  for (std::int32_t e : w)
    if (auto a = events[e].letter_for(leaf)) out.push_back(*a);
  return out;
}

namespace {

struct vec_hash {
  std::size_t operator()(const std::vector<std::int32_t>& v) const noexcept {
    std::size_t h = 0;
    for (std::int32_t x : v) util::hash_combine(h, x);
    return h;
  }
};

}  // namespace

verdict mono(const model& m, std::int64_t cap) {
  const std::int32_t n = static_cast<std::int32_t>(m.leaves.size());
  std::unordered_map<std::vector<std::int32_t>, std::int64_t, vec_hash> seen;
  std::vector<std::vector<std::int32_t>> states;
  std::vector<std::pair<std::int64_t, std::int32_t>> pred;  // (state, event)
  auto rebuild = [&](std::int64_t s) {
    eword w;
    for (; pred[s].second >= 0; s = pred[s].first)
      w.push_back(pred[s].second);
    return eword(w.rbegin(), w.rend());
  };

  std::vector<std::int32_t> init(static_cast<std::size_t>(n) + 1, 0);
  seen.emplace(init, 0);
  states.push_back(init);
  pred.emplace_back(-1, -1);
  verdict v;
  if (m.mon.bad[0]) {
    v.k = verdict::kind::violation;
    v.states_walked = 1;
    return v;
  }
  for (std::int64_t head = 0; head < static_cast<std::int64_t>(states.size());
       ++head) {
    const auto cur = states[head];  // copy: `states` reallocates below
    for (std::int32_t e = 0; e < m.mon.n_events; ++e) {
      std::vector<std::int32_t> nxt = cur;
      nxt[0] = m.mon.step(cur[0], e);
      bool enabled = true;
      for (const auto& [l, a] : m.events[e].support) {
        std::int32_t q = m.leaves[l].step(cur[l + 1], a);
        if (q < 0) {
          enabled = false;
          break;
        }
        nxt[l + 1] = q;
      }
      if (!enabled) continue;
      auto [it, fresh] =
          seen.emplace(nxt, static_cast<std::int64_t>(states.size()));
      if (!fresh) continue;
      states.push_back(nxt);
      pred.emplace_back(head, e);
      if (m.mon.bad[nxt[0]]) {
        v.k = verdict::kind::violation;
        v.witness = rebuild(static_cast<std::int64_t>(states.size()) - 1);
        v.states_walked = static_cast<std::int64_t>(states.size());
        return v;
      }
      if (static_cast<std::int64_t>(states.size()) > cap) {
        v.k = verdict::kind::cap;
        v.states_walked = static_cast<std::int64_t>(states.size());
        return v;
      }
    }
  }
  v.k = verdict::kind::holds;
  v.states_walked = static_cast<std::int64_t>(states.size());
  return v;
}

bool refire(const model& m, const eword& w) {
  std::int32_t mm = 0;
  for (std::int32_t e : w) mm = m.mon.step(mm, e);
  if (!m.mon.bad[mm]) return false;
  for (std::int32_t i = 0; i < static_cast<std::int32_t>(m.leaves.size()); ++i)
    if (!m.leaves[i].fire(m.project(w, i))) return false;
  return true;
}

// ---------------------------------------------------------------- .cts I/O

namespace {

std::int32_t find_name(const std::vector<std::string>& names,
                       const std::string& s) {
  for (std::size_t i = 0; i < names.size(); ++i)
    if (names[i] == s) return static_cast<std::int32_t>(i);
  return -1;
}

/// Parse a shape s-expression over leaf names; -1 on error.
std::int32_t parse_sexpr(std::istringstream& in, model& m) {
  std::string tok;
  if (!(in >> tok)) return -1;
  if (tok == "(") {
    std::int32_t l = parse_sexpr(in, m);
    std::int32_t r = parse_sexpr(in, m);
    if (l < 0 || r < 0 || !(in >> tok) || tok != ")") return -1;
    m.tree.nodes.push_back({-1, l, r});
    return static_cast<std::int32_t>(m.tree.nodes.size()) - 1;
  }
  std::int32_t leaf = find_name(m.leaf_names, tok);
  if (leaf < 0) return -1;
  m.tree.nodes.push_back({leaf, -1, -1});
  return static_cast<std::int32_t>(m.tree.nodes.size()) - 1;
}

}  // namespace

std::optional<model> parse_cts(const std::string& text, std::string* err) {
  auto fail = [err](int line, const std::string& msg) -> std::optional<model> {
    if (err) *err = "line " + std::to_string(line) + ": " + msg;
    return std::nullopt;
  };
  model m;
  m.mon.n_states = 0;
  std::vector<std::string> event_names;
  // Monitor transitions buffered until the event count is known.
  std::vector<std::array<std::int32_t, 3>> mtrans;
  std::vector<std::int32_t> bads;
  std::int32_t cur = -1;  // leaf under construction
  bool saw_header = false, saw_monitor = false;
  std::istringstream in(text);
  std::string raw;
  int lineno = 0;
  while (std::getline(in, raw)) {
    ++lineno;
    if (auto h = raw.find('#'); h != std::string::npos) raw.resize(h);
    std::istringstream ls(raw);
    std::string kw;
    if (!(ls >> kw)) continue;
    if (kw == "cts") {
      int v;
      if (!(ls >> v) || v != 1) return fail(lineno, "expected 'cts 1'");
      saw_header = true;
    } else if (!saw_header) {
      return fail(lineno, "missing 'cts 1' header");
    } else if (kw == "leaf") {
      std::string name, kw1, kw2;
      std::int32_t n, k;
      if (!(ls >> name >> kw1 >> n >> kw2 >> k) || kw1 != "states" ||
          kw2 != "letters" || n < 1 || k < 0)
        return fail(lineno, "bad leaf declaration");
      if (find_name(m.leaf_names, name) >= 0)
        return fail(lineno, "duplicate leaf " + name);
      m.leaf_names.push_back(name);
      m.leaves.push_back(
          {n, k, std::vector<std::int32_t>(
                     static_cast<std::size_t>(n) * k, -1)});
      cur = static_cast<std::int32_t>(m.leaves.size()) - 1;
    } else if (kw == "t") {
      std::int32_t s, a, d;
      if (cur < 0 || !(ls >> s >> a >> d)) return fail(lineno, "bad t line");
      lts& l = m.leaves[cur];
      if (s < 0 || s >= l.n_states || a < 0 || a >= l.n_letters || d < 0 ||
          d >= l.n_states)
        return fail(lineno, "t out of range");
      std::int32_t& slot = l.delta[static_cast<std::size_t>(s) * l.n_letters + a];
      if (slot != -1) return fail(lineno, "duplicate transition");
      slot = d;
    } else if (kw == "shape") {
      std::string rest;
      std::getline(ls, rest);
      std::string spaced;
      for (char c : rest)
        if (c == '(' || c == ')') {
          spaced += ' ';
          spaced += c;
          spaced += ' ';
        } else {
          spaced += c;
        }
      std::istringstream ss(spaced);
      m.tree.nodes.clear();
      m.tree.root = parse_sexpr(ss, m);
      std::string tail;
      if (m.tree.root < 0 || (ss >> tail))
        return fail(lineno, "bad shape s-expression");
    } else if (kw == "event") {
      std::string name;
      if (!(ls >> name)) return fail(lineno, "bad event line");
      if (find_name(event_names, name) >= 0)
        return fail(lineno, "duplicate event " + name);
      event e;
      e.name = name;
      std::string pair;
      while (ls >> pair) {
        auto colon = pair.find(':');
        if (colon == std::string::npos) return fail(lineno, "bad support pair");
        std::int32_t leaf = find_name(m.leaf_names, pair.substr(0, colon));
        if (leaf < 0) return fail(lineno, "unknown leaf in support");
        std::int32_t a;
        try {
          a = std::stoi(pair.substr(colon + 1));
        } catch (...) {
          return fail(lineno, "bad letter");
        }
        if (a < 0 || a >= m.leaves[leaf].n_letters)
          return fail(lineno, "letter out of range");
        if (e.letter_for(leaf)) return fail(lineno, "leaf twice in support");
        e.support.emplace_back(leaf, static_cast<letter>(a));
      }
      if (e.support.empty()) return fail(lineno, "empty support");
      std::sort(e.support.begin(), e.support.end());
      event_names.push_back(name);
      m.events.push_back(std::move(e));
    } else if (kw == "monitor") {
      std::string kw1;
      std::int32_t n;
      if (!(ls >> kw1 >> n) || kw1 != "states" || n < 1)
        return fail(lineno, "bad monitor declaration");
      m.mon.n_states = n;
      saw_monitor = true;
    } else if (kw == "m") {
      std::int32_t s, d;
      std::string ev;
      if (!saw_monitor || !(ls >> s >> ev >> d))
        return fail(lineno, "bad m line");
      std::int32_t e = find_name(event_names, ev);
      if (e < 0 || s < 0 || s >= m.mon.n_states || d < 0 || d >= m.mon.n_states)
        return fail(lineno, "m out of range");
      mtrans.push_back({s, e, d});
    } else if (kw == "bad") {
      std::int32_t s;
      while (ls >> s) {
        if (s < 0 || s >= m.mon.n_states) return fail(lineno, "bad out of range");
        bads.push_back(s);
      }
    } else {
      return fail(lineno, "unknown keyword " + kw);
    }
  }
  if (!saw_monitor) return fail(lineno, "missing monitor");
  m.mon.n_events = static_cast<std::int32_t>(m.events.size());
  // Unlisted monitor transitions self-loop (stutter on irrelevant events).
  m.mon.delta.assign(
      static_cast<std::size_t>(m.mon.n_states) * m.mon.n_events, 0);
  for (std::int32_t s = 0; s < m.mon.n_states; ++s)
    for (std::int32_t e = 0; e < m.mon.n_events; ++e)
      m.mon.delta[static_cast<std::size_t>(s) * m.mon.n_events + e] = s;
  for (const auto& [s, e, d] : mtrans)
    m.mon.delta[static_cast<std::size_t>(s) * m.mon.n_events + e] = d;
  m.mon.bad.assign(m.mon.n_states, false);
  for (std::int32_t s : bads) m.mon.bad[s] = true;
  if (m.tree.root < 0)
    m.tree = shape::comb(static_cast<std::int32_t>(m.leaves.size()));
  return m;
}

std::string print_cts(const model& m) {
  std::ostringstream out;
  out << "cts 1\n";
  for (std::size_t i = 0; i < m.leaves.size(); ++i) {
    const lts& l = m.leaves[i];
    out << "leaf " << m.leaf_names[i] << " states " << l.n_states
        << " letters " << l.n_letters << "\n";
    for (std::int32_t s = 0; s < l.n_states; ++s)
      for (std::int32_t a = 0; a < l.n_letters; ++a)
        if (l.step(s, static_cast<letter>(a)) >= 0)
          out << "t " << s << " " << a << " "
              << l.step(s, static_cast<letter>(a)) << "\n";
  }
  for (const event& e : m.events) {
    out << "event " << e.name;
    for (const auto& [l, a] : e.support)
      out << " " << m.leaf_names[l] << ":" << a;
    out << "\n";
  }
  out << "monitor states " << m.mon.n_states << "\n";
  for (std::int32_t s = 0; s < m.mon.n_states; ++s)
    for (std::int32_t e = 0; e < m.mon.n_events; ++e)
      if (m.mon.step(s, e) != s)
        out << "m " << s << " " << m.events[e].name << " " << m.mon.step(s, e)
            << "\n";
  bool any = false;
  for (std::int32_t s = 0; s < m.mon.n_states; ++s)
    if (m.mon.bad[s]) {
      out << (any ? " " : "bad ") << s;
      any = true;
    }
  if (any) out << "\n";
  return out.str();
}

}  // namespace hsc::cegar
