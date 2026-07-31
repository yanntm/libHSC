/// \file cegar/model.cc — leaves, events, monitor; the monolithic oracle
/// walk and concrete replay.

#include "hsc/cegar/model.hh"

#include <algorithm>
#include <cstring>
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

word model::project(const eword& w, std::int32_t leaf) const {
  word out;
  for (std::int32_t e : w)
    if (auto a = events[e].letter_for(leaf)) out.push_back(*a);
  return out;
}

bool model::is_prop(std::int32_t leaf) const {
  return std::binary_search(prop_leaves.begin(), prop_leaves.end(), leaf);
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
      if (nxt[0] < 0) continue;  // a property leaf blocks
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
  for (std::int32_t e : w) {
    mm = m.mon.step(mm, e);
    if (mm < 0) return false;
  }
  if (!m.mon.bad[mm]) return false;
  for (std::int32_t i = 0; i < static_cast<std::int32_t>(m.leaves.size()); ++i)
    if (!m.leaves[i].fire(m.project(w, i))) return false;
  return true;
}

}  // namespace hsc::cegar
