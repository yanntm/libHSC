/// \file hsc-certcheck.cc — the independent certificate checker.
///
/// Deliberately self-contained: its own `.cts` and `.cert` parsers and
/// its own two walks, sharing no code with the CEGAR library it checks.
/// Obligations (spec §6): per distinct leaf, the inclusion walk finds no
/// reachable (state, dead-class) pair; globally, Inv contains the
/// initial abstract state, is closed under every event (an event blocked
/// by a dead successor class counts as closed), and avoids bad monitor
/// states. Exit 0 iff every obligation passes.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Leaf {
  std::string name;
  std::int32_t n_states = 0, n_letters = 0;
  std::vector<std::int32_t> delta;  // n_states x n_letters, -1 undefined
};

struct Event {
  std::string name;
  std::vector<std::pair<std::int32_t, std::int32_t>> support;  // leaf, letter
};

struct Table {
  std::int32_t k = 0, dead = -1, n_letters = 0;
  std::vector<std::int32_t> act;  // k x n_letters, -1 = unset
};

struct Input {
  std::vector<Leaf> leaves;
  std::vector<Event> events;
  std::int32_t mon_states = 0;
  std::vector<std::int32_t> mon_delta;  // states x events (self-loop default)
  std::vector<bool> mon_bad;
  std::vector<Table> tables;            // one per leaf instance (aliased)
  std::vector<std::vector<std::int32_t>> inv;
};

int fail(const std::string& msg) {
  std::fprintf(stderr, "certcheck: %s\n", msg.c_str());
  return 1;
}

std::int32_t leaf_index(const Input& in, const std::string& name) {
  for (std::size_t i = 0; i < in.leaves.size(); ++i)
    if (in.leaves[i].name == name) return static_cast<std::int32_t>(i);
  return -1;
}

bool parse_model(const std::string& text, Input& in, std::string& err) {
  std::istringstream is(text);
  std::string raw;
  std::int32_t cur = -1;
  std::vector<std::array<std::int32_t, 3>> mtrans;
  std::vector<std::int32_t> bads;
  std::map<std::string, std::int32_t> event_ids;
  while (std::getline(is, raw)) {
    if (auto h = raw.find('#'); h != std::string::npos) raw.resize(h);
    std::istringstream ls(raw);
    std::string kw;
    if (!(ls >> kw)) continue;
    if (kw == "cts" || kw == "shape") continue;  // shape is irrelevant here
    if (kw == "leaf") {
      Leaf l;
      std::string k1, k2;
      if (!(ls >> l.name >> k1 >> l.n_states >> k2 >> l.n_letters)) {
        err = "bad leaf";
        return false;
      }
      l.delta.assign(
          static_cast<std::size_t>(l.n_states) * l.n_letters, -1);
      in.leaves.push_back(std::move(l));
      cur = static_cast<std::int32_t>(in.leaves.size()) - 1;
    } else if (kw == "t") {
      std::int32_t s, a, d;
      if (cur < 0 || !(ls >> s >> a >> d)) {
        err = "bad t";
        return false;
      }
      Leaf& l = in.leaves[cur];
      l.delta[static_cast<std::size_t>(s) * l.n_letters + a] = d;
    } else if (kw == "event") {
      Event e;
      if (!(ls >> e.name)) {
        err = "bad event";
        return false;
      }
      std::string pair;
      while (ls >> pair) {
        auto c = pair.find(':');
        std::int32_t lf = leaf_index(in, pair.substr(0, c));
        if (c == std::string::npos || lf < 0) {
          err = "bad support";
          return false;
        }
        e.support.emplace_back(lf, std::stoi(pair.substr(c + 1)));
      }
      event_ids[e.name] = static_cast<std::int32_t>(in.events.size());
      in.events.push_back(std::move(e));
    } else if (kw == "monitor") {
      std::string k1;
      if (!(ls >> k1 >> in.mon_states)) {
        err = "bad monitor";
        return false;
      }
    } else if (kw == "m") {
      std::int32_t s, d;
      std::string ev;
      if (!(ls >> s >> ev >> d) || !event_ids.count(ev)) {
        err = "bad m";
        return false;
      }
      mtrans.push_back({s, event_ids[ev], d});
    } else if (kw == "bad") {
      std::int32_t s;
      while (ls >> s) bads.push_back(s);
    } else {
      err = "unknown keyword " + kw;
      return false;
    }
  }
  const std::int32_t ne = static_cast<std::int32_t>(in.events.size());
  in.mon_delta.assign(static_cast<std::size_t>(in.mon_states) * ne, 0);
  for (std::int32_t s = 0; s < in.mon_states; ++s)
    for (std::int32_t e = 0; e < ne; ++e)
      in.mon_delta[static_cast<std::size_t>(s) * ne + e] = s;
  for (auto& t : mtrans)
    in.mon_delta[static_cast<std::size_t>(t[0]) * ne + t[1]] = t[2];
  in.mon_bad.assign(in.mon_states, false);
  for (std::int32_t s : bads) in.mon_bad[s] = true;
  return true;
}

bool parse_cert(const std::string& text, Input& in, std::string& err) {
  in.tables.assign(in.leaves.size(), Table{});
  std::vector<bool> have(in.leaves.size(), false);
  std::istringstream is(text);
  std::string raw;
  std::int32_t cur = -1;
  std::vector<std::pair<std::int32_t, std::int32_t>> aliases;
  while (std::getline(is, raw)) {
    if (auto h = raw.find('#'); h != std::string::npos) raw.resize(h);
    std::istringstream ls(raw);
    std::string kw;
    if (!(ls >> kw)) continue;
    if (kw == "cert") continue;
    if (kw == "classifier") {
      std::string name, k1, k2;
      std::int32_t k, dead;
      if (!(ls >> name >> k1 >> k >> k2 >> dead)) {
        err = "bad classifier";
        return false;
      }
      cur = leaf_index(in, name);
      if (cur < 0 || k < 1 || dead < -1 || dead >= k) {
        err = "bad classifier header";
        return false;
      }
      Table& t = in.tables[cur];
      t.k = k;
      t.dead = dead;
      t.n_letters = in.leaves[cur].n_letters;
      t.act.assign(static_cast<std::size_t>(k) * t.n_letters, -1);
      have[cur] = true;
    } else if (kw == "a") {
      std::int32_t c, a, d;
      if (cur < 0 || !(ls >> c >> a >> d)) {
        err = "bad a line";
        return false;
      }
      Table& t = in.tables[cur];
      if (c < 0 || c >= t.k || a < 0 || a >= t.n_letters || d < 0 || d >= t.k) {
        err = "a out of range";
        return false;
      }
      t.act[static_cast<std::size_t>(c) * t.n_letters + a] = d;
    } else if (kw == "use") {
      std::string inst, repr;
      if (!(ls >> inst >> repr)) {
        err = "bad use";
        return false;
      }
      std::int32_t li = leaf_index(in, inst), lr = leaf_index(in, repr);
      if (li < 0 || lr < 0) {
        err = "unknown leaf in use";
        return false;
      }
      aliases.emplace_back(li, lr);
    } else if (kw == "inv") {
      std::vector<std::int32_t> st;
      std::int32_t x;
      while (ls >> x) st.push_back(x);
      if (st.size() != in.leaves.size() + 1) {
        err = "inv arity";
        return false;
      }
      in.inv.push_back(std::move(st));
    } else {
      err = "unknown cert keyword " + kw;
      return false;
    }
  }
  for (auto& [li, lr] : aliases) {
    if (!have[lr]) {
      err = "use of leaf without classifier";
      return false;
    }
    // An aliased instance must be byte-identical as an LTS: the
    // obligation (L) is checked once, on the representative.
    if (in.leaves[li].delta != in.leaves[lr].delta ||
        in.leaves[li].n_letters != in.leaves[lr].n_letters) {
      err = "use aliases non-identical leaves";
      return false;
    }
    in.tables[li] = in.tables[lr];
    have[li] = true;
  }
  for (std::size_t i = 0; i < in.leaves.size(); ++i) {
    if (!have[i]) {
      err = "leaf without classifier: " + in.leaves[i].name;
      return false;
    }
    for (std::int32_t x : in.tables[i].act)
      if (x < 0) {
        err = "incomplete action table for " + in.leaves[i].name;
        return false;
      }
  }
  return true;
}

/// Obligation (L_i): no reachable (state, dead class) pair.
bool check_inclusion(const Leaf& l, const Table& t) {
  if (t.dead < 0) return true;  // everything live: nothing to reach
  std::vector<bool> seen(static_cast<std::size_t>(l.n_states) * t.k, false);
  std::vector<std::int64_t> queue;
  auto id = [&](std::int32_t q, std::int32_t c) {
    return static_cast<std::int64_t>(q) * t.k + c;
  };
  seen[id(0, 0)] = true;
  queue.push_back(id(0, 0));
  if (t.dead == 0) return false;  // ε rejected, but ε is a trace
  for (std::size_t head = 0; head < queue.size(); ++head) {
    std::int64_t p = queue[head];
    std::int32_t q = static_cast<std::int32_t>(p / t.k);
    std::int32_t c = static_cast<std::int32_t>(p % t.k);
    for (std::int32_t a = 0; a < l.n_letters; ++a) {
      std::int32_t q2 = l.delta[static_cast<std::size_t>(q) * l.n_letters + a];
      if (q2 < 0) continue;
      std::int32_t c2 = t.act[static_cast<std::size_t>(c) * t.n_letters + a];
      if (c2 == t.dead) return false;
      if (seen[id(q2, c2)]) continue;
      seen[id(q2, c2)] = true;
      queue.push_back(id(q2, c2));
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: hsc-certcheck model.cts proof.cert\n");
    return 2;
  }
  auto slurp = [](const char* path, std::string& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
  };
  std::string mtext, ctext, err;
  if (!slurp(argv[1], mtext)) return fail("cannot open model");
  if (!slurp(argv[2], ctext)) return fail("cannot open certificate");
  Input in;
  if (!parse_model(mtext, in, err)) return fail("model: " + err);
  if (!parse_cert(ctext, in, err)) return fail("cert: " + err);

  // (L_i) per leaf — aliased instances point at identical leaves, so
  // re-checking duplicates is redundant but harmless; skip via a set of
  // checked delta signatures is not worth the code: check all.
  for (std::size_t i = 0; i < in.leaves.size(); ++i)
    if (!check_inclusion(in.leaves[i], in.tables[i]))
      return fail("obligation L failed for leaf " + in.leaves[i].name);

  // (G1) the initial abstract state.
  std::set<std::vector<std::int32_t>> inv(in.inv.begin(), in.inv.end());
  std::vector<std::int32_t> init(in.leaves.size() + 1, 0);
  if (!inv.count(init)) return fail("obligation G1 failed: initial not in Inv");

  // (G3) no bad monitor coordinate; (G2) closure under every event.
  const std::int32_t ne = static_cast<std::int32_t>(in.events.size());
  for (const auto& st : inv) {
    if (st[0] < 0 || st[0] >= in.mon_states)
      return fail("Inv state with monitor coordinate out of range");
    if (in.mon_bad[st[0]]) return fail("obligation G3 failed: bad state in Inv");
    for (std::size_t i = 0; i < in.leaves.size(); ++i)
      if (st[i + 1] < 0 || st[i + 1] >= in.tables[i].k)
        return fail("Inv state with class out of range");
    for (std::int32_t e = 0; e < ne; ++e) {
      std::vector<std::int32_t> nxt = st;
      nxt[0] = in.mon_delta[static_cast<std::size_t>(st[0]) * ne + e];
      bool enabled = true;
      for (auto& [lf, a] : in.events[e].support) {
        const Table& t = in.tables[lf];
        std::int32_t c2 =
            t.act[static_cast<std::size_t>(st[lf + 1]) * t.n_letters + a];
        if (c2 == t.dead) {
          enabled = false;
          break;
        }
        nxt[lf + 1] = c2;
      }
      if (enabled && !inv.count(nxt))
        return fail("obligation G2 failed: Inv not closed under " +
                    in.events[e].name);
    }
  }
  std::printf("certcheck: all obligations pass (%zu leaves, %zu Inv states)\n",
              in.leaves.size(), in.inv.size());
  return 0;
}
