/// \file cegar/loop.cc — the verification loop: abstract-product search,
/// replay by projection, refine-until-resolved, certificate emission.

#include "hsc/cegar/loop.hh"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>

#include "hsc/cegar/certify.hh"
#include "hsc/cegar/learner.hh"
#include "hsc/util/hash.hh"

namespace hsc::cegar {

namespace {

[[noreturn]] void die(const char* what) {
  std::fprintf(stderr, "cegar loop invariant violated: %s\n", what);
  std::abort();
}

struct vec_hash {
  std::size_t operator()(const std::vector<std::int32_t>& v) const noexcept {
    std::size_t h = 0;
    for (std::int32_t x : v) util::hash_combine(h, x);
    return h;
  }
};

/// One entry per distinct non-property leaf: shared learner, classifier,
/// budget. Property leaves have no entry (of_leaf == -1): the monitor
/// tracks them exactly (spec §5.6).
struct profile {
  std::vector<std::unique_ptr<learner>> entries;
  std::vector<std::int64_t> entry_states;  // |Q| of the entry's leaf
  std::vector<std::int32_t> entry_leaf;    // entry -> representative leaf
  std::vector<std::int32_t> of_leaf;       // leaf instance -> entry, -1 prop

  static profile build(const model& m, bool intern) {
    profile p;
    std::map<std::string, std::int32_t> by_key;
    for (std::size_t i = 0; i < m.leaves.size(); ++i) {
      if (m.is_prop(static_cast<std::int32_t>(i))) {
        p.of_leaf.push_back(-1);
        continue;
      }
      std::int32_t entry = -1;
      if (intern) {
        auto [it, fresh] = by_key.try_emplace(
            m.leaves[i].serialize(),
            static_cast<std::int32_t>(p.entries.size()));
        entry = it->second;
        if (!fresh) {
          p.of_leaf.push_back(entry);
          continue;
        }
      } else {
        entry = static_cast<std::int32_t>(p.entries.size());
      }
      p.entries.push_back(std::make_unique<learner>(m.leaves[i]));
      p.entry_states.push_back(m.leaves[i].n_states);
      p.entry_leaf.push_back(static_cast<std::int32_t>(i));
      p.of_leaf.push_back(entry);
    }
    return p;
  }

  [[nodiscard]] std::int64_t budget() const {
    std::int64_t b = 0;
    for (std::int64_t q : entry_states) b += q + 1;
    return b;
  }
  [[nodiscard]] std::int64_t cex_total() const {
    std::int64_t c = 0;
    for (const auto& e : entries) c += e->counterexamples();
    return c;
  }
  [[nodiscard]] std::int64_t index_total() {
    std::int64_t s = 0;
    for (const auto& e : entries) s += e->published().k;
    return s;
  }
};

/// Search result: a witness, or Inv (the reached set) proving "holds".
/// Abstract states are (m, x_1..x_n) with x_i the class of leaf i's
/// entry, constant 0 for property leaves (the monitor carries them).
struct search_result {
  bool bad_found = false;
  bool capped = false;  ///< the cap was hit: a resource verdict, not a bug
  eword witness;
  std::vector<std::vector<std::int32_t>> inv;
};

search_result search(const model& m, profile& p, std::int64_t cap) {
  const std::int32_t n = static_cast<std::int32_t>(m.leaves.size());
  std::vector<const classifier*> h(m.leaves.size(), nullptr);
  for (std::int32_t i = 0; i < n; ++i)
    if (p.of_leaf[i] >= 0) h[i] = &p.entries[p.of_leaf[i]]->published();
  std::unordered_map<std::vector<std::int32_t>, std::int64_t, vec_hash> seen;
  search_result r;
  std::vector<std::pair<std::int64_t, std::int32_t>> pred;
  auto rebuild = [&](std::int64_t s) {
    eword w;
    for (; pred[s].second >= 0; s = pred[s].first)
      w.push_back(pred[s].second);
    return eword(w.rbegin(), w.rend());
  };
  std::vector<std::int32_t> init(static_cast<std::size_t>(n) + 1, 0);
  seen.emplace(init, 0);
  r.inv.push_back(init);
  pred.emplace_back(-1, -1);
  if (m.mon.bad[0]) {
    r.bad_found = true;
    return r;
  }
  for (std::int64_t head = 0; head < static_cast<std::int64_t>(r.inv.size());
       ++head) {
    const auto cur = r.inv[head];  // copy: r.inv reallocates below
    for (std::int32_t e = 0; e < m.mon.n_events; ++e) {
      std::vector<std::int32_t> nxt = cur;
      nxt[0] = m.mon.step(cur[0], e);
      if (nxt[0] < 0) continue;  // a property leaf blocks
      bool enabled = true;
      for (const auto& [l, a] : m.events[e].support) {
        if (!h[l]) continue;  // property leaf: the monitor answered
        std::int32_t c = h[l]->step(cur[l + 1], a);
        if (!h[l]->live(c)) {
          enabled = false;
          break;
        }
        nxt[l + 1] = c;
      }
      if (!enabled) continue;
      auto [it, fresh] =
          seen.emplace(nxt, static_cast<std::int64_t>(r.inv.size()));
      if (!fresh) continue;
      r.inv.push_back(nxt);
      pred.emplace_back(head, e);
      if (m.mon.bad[nxt[0]]) {
        r.bad_found = true;
        r.witness = rebuild(static_cast<std::int64_t>(r.inv.size()) - 1);
        return r;
      }
      if (static_cast<std::int64_t>(r.inv.size()) > cap) {
        // A resource limit, not a loop invariant: an abstraction whose
        // product outgrows the cap (many fine leaves, ghost states) is
        // the caller's to diagnose — report, never abort.
        r.capped = true;
        return r;
      }
    }
  }
  return r;
}

/// Replay by projection; collects failing leaves. A property-leaf
/// projection cannot fail (the monitor is exact): asserted.
std::vector<std::int32_t> replay(const model& m, const eword& w) {
  std::vector<std::int32_t> culprits;
  for (std::int32_t i = 0; i < static_cast<std::int32_t>(m.leaves.size());
       ++i) {
    word u = m.project(w, i);
    if (u.empty()) continue;  // no support here: skip wholesale
    if (m.leaves[i].fire(u)) continue;
    if (m.is_prop(i)) die("property-leaf projection failed on replay");
    culprits.push_back(i);
  }
  return culprits;
}

/// Refine entry until certified and rejecting u; feeds certification
/// positives back. Every processed word burns budget (asserted).
void refine_until_resolved(learner& l, const lts& leaf, const word& u,
                           std::int64_t leaf_budget) {
  for (;;) {
    if (l.counterexamples() > leaf_budget) die("budget exceeded");
    if (!l.published().accepts(u)) {
      // Still must be certified before use.
      cert_result c = certify(leaf, l.published());
      if (c.certified) return;
      l.add_counterexample(c.counterexample);
      continue;
    }
    l.add_counterexample(u);
    for (;;) {
      cert_result c = certify(leaf, l.published());
      if (c.certified) break;
      if (l.counterexamples() > leaf_budget) die("budget exceeded");
      l.add_counterexample(c.counterexample);
    }
    if (!l.published().accepts(u)) return;
  }
}

/// A live-classified word the leaf cannot fire — a witness that L(H)
/// exceeds the leaf's language — found by walking H against the
/// sink-completed leaf. nullopt means the inclusion L(H) ⊆ L holds.
std::optional<word> reverse_gap(const lts& leaf, const classifier& h) {
  const std::int32_t sink = leaf.n_states;
  const std::int32_t width = leaf.n_states + 1;
  std::vector<bool> seen(static_cast<std::size_t>(h.k) * width, false);
  std::vector<std::pair<std::int32_t, letter>> pred(seen.size(), {-1, 0});
  std::vector<std::int32_t> queue;
  auto id = [&](std::int32_t c, std::int32_t q) { return c * width + q; };
  seen[id(0, 0)] = true;
  queue.push_back(id(0, 0));
  for (std::size_t head = 0; head < queue.size(); ++head) {
    std::int32_t pr = queue[head];
    std::int32_t c = pr / width, q = pr % width;
    for (std::int32_t a = 0; a < leaf.n_letters; ++a) {
      std::int32_t c2 = h.step(c, static_cast<letter>(a));
      if (!h.live(c2)) continue;
      std::int32_t q2 = q == sink ? sink : leaf.step(q, static_cast<letter>(a));
      if (q2 < 0) q2 = sink;
      std::int32_t p2 = id(c2, q2);
      if (seen[p2]) continue;
      seen[p2] = true;
      pred[p2] = {pr, static_cast<letter>(a)};
      queue.push_back(p2);
      if (q2 == sink) {
        word w;
        for (std::int32_t p = p2; pred[p].first >= 0; p = pred[p].first)
          w.push_back(pred[p].second);
        return word(w.rbegin(), w.rend());
      }
    }
  }
  return std::nullopt;
}

/// Drive an entry to its exact rung: alternate certification (inclusion
/// one way) with the reverse-gap walk (inclusion the other way).
void drive_exact(learner& l, const lts& leaf, std::int64_t leaf_budget) {
  for (;;) {
    if (l.counterexamples() > leaf_budget + 1) die("budget exceeded");
    cert_result c = certify(leaf, l.published());
    if (!c.certified) {
      l.add_counterexample(c.counterexample);
      continue;
    }
    std::optional<word> gap = reverse_gap(leaf, l.published());
    if (!gap) return;  // languages equal: the exact rung
    l.add_counterexample(*gap);
  }
}

/// The certificate document (spec §6): `.hsc` s-expressions. One
/// `classifier` per distinct non-property leaf, `use` aliases for the
/// interned instances, one `inv` per abstract state — property leaves
/// as model values, others as class indices.
std::string emit_certificate(const model& m, const profile& p,
                             const std::vector<std::vector<std::int32_t>>& inv,
                             const std::vector<const classifier*>& h) {
  std::ostringstream out;
  out << "(certificate (select " << m.property_text << "))\n";
  std::vector<std::int32_t> rep_leaf(p.entries.size(), -1);
  for (std::size_t i = 0; i < m.leaves.size(); ++i) {
    const std::int32_t entry = p.of_leaf[i];
    if (entry < 0) continue;  // property leaf: the monitor carries it
    if (rep_leaf[entry] < 0) {
      rep_leaf[entry] = static_cast<std::int32_t>(i);
      const classifier& c = *h[i];
      out << "(classifier " << m.leaf_names[i] << ' ' << c.k << ' ' << c.dead;
      for (std::int32_t cl = 0; cl < c.k; ++cl)
        for (std::int32_t a = 0; a < c.n_letters; ++a)
          out << "\n  (a " << cl << ' ' << a << ' '
              << c.step(cl, static_cast<letter>(a)) << ')';
      out << ")\n";
    } else {
      out << "(use " << m.leaf_names[i] << ' '
          << m.leaf_names[rep_leaf[entry]] << ")\n";
    }
  }
  for (const auto& st : inv) {
    out << "(inv";
    const auto& vals = m.monitor_values[st[0]];
    for (std::size_t i = 0; i < m.leaves.size(); ++i) {
      const std::int32_t x =
          p.of_leaf[i] >= 0
              ? st[i + 1]
              : vals[static_cast<std::size_t>(
                    std::lower_bound(m.prop_leaves.begin(),
                                     m.prop_leaves.end(),
                                     static_cast<std::int32_t>(i)) -
                    m.prop_leaves.begin())];
      out << " (" << m.leaf_names[i] << ' ' << x << ')';
    }
    out << ")\n";
  }
  return out.str();
}

}  // namespace

run_result run(const model& m, const options& opt) {
  profile p = profile::build(m, opt.intern);
  run_result r;
  r.budget = p.budget();
  std::int64_t last_index = p.index_total();
  const auto now = [] { return std::chrono::steady_clock::now(); };
  const auto ns = [](auto d) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
  };
  for (;;) {
    ++r.rounds;
    auto t0 = now();
    search_result s = search(m, p, opt.cap);
    r.ns_search += ns(now() - t0);
    if (s.capped) {
      r.v.k = verdict::kind::cap;
      r.v.states_walked = static_cast<std::int64_t>(s.inv.size());
      break;
    }
    std::vector<const classifier*> h(m.leaves.size(), nullptr);
    for (std::size_t i = 0; i < m.leaves.size(); ++i)
      if (p.of_leaf[i] >= 0) h[i] = &p.entries[p.of_leaf[i]]->published();
    if (!s.bad_found) {
      r.v.k = verdict::kind::holds;
      r.v.states_walked = static_cast<std::int64_t>(s.inv.size());
      r.inv_size = static_cast<std::int64_t>(s.inv.size());
      r.certificate = emit_certificate(m, p, s.inv, h);
      break;
    }
    auto t1 = now();
    std::vector<std::int32_t> culprits = replay(m, s.witness);
    r.ns_replay += ns(now() - t1);
    if (culprits.empty()) {
      r.v.k = verdict::kind::violation;
      r.v.witness = s.witness;
      r.v.states_walked = static_cast<std::int64_t>(s.inv.size());
      r.final_values.reserve(m.leaves.size());
      for (std::size_t i = 0; i < m.leaves.size(); ++i) {
        const lts& l = m.leaves[i];
        const auto q = l.fire(m.project(s.witness,
                                        static_cast<std::int32_t>(i)));
        if (!q) die("validated witness stopped refiring");
        r.final_values.push_back(l.value_of[static_cast<std::size_t>(*q)]);
      }
      break;
    }
    // Policy: which culprits of this witness to refine.
    if (opt.pick == options::culprits::first) {
      culprits.resize(1);
    } else if (opt.pick == options::culprits::cheapest) {
      std::int32_t best = culprits[0];
      for (std::int32_t c : culprits)
        if (h[c]->k < h[best]->k) best = c;
      culprits.assign(1, best);
    }
    // Interned duplicates in one batch: refine each entry once per round.
    auto t2 = now();
    std::vector<bool> entry_done(p.entries.size(), false);
    for (std::int32_t c : culprits) {
      std::int32_t entry = p.of_leaf[c];
      if (entry_done[entry]) continue;
      entry_done[entry] = true;
      learner& l = *p.entries[entry];
      word u = m.project(s.witness, c);
      refine_until_resolved(l, m.leaves[c], u, p.entry_states[entry] + 1);
      if (opt.jump_exact)
        drive_exact(l, m.leaves[c], p.entry_states[entry] + 1);
      if (l.published().accepts(u)) die("refined witness still accepted");
    }
    r.ns_refine += ns(now() - t2);
    std::int64_t idx = p.index_total();
    if (idx <= last_index) die("no progress in refinement round");
    last_index = idx;
    if (p.cex_total() > r.budget) die("global budget exceeded");
  }
  r.cex_total = p.cex_total();
  for (std::size_t e = 0; e < p.entries.size(); ++e) {
    const classifier& c = p.entries[e]->published();
    const lts& leaf = m.leaves[p.entry_leaf[e]];
    if (c.k == 1 && c.dead < 0)
      ++r.leaves_chaotic;
    else if (!reverse_gap(leaf, c))
      ++r.leaves_exact;  // certified (loop invariant) + no reverse gap
    else
      ++r.leaves_intermediate;
  }
  return r;
}

}  // namespace hsc::cegar
