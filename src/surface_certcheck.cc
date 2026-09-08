/// \file surface_certcheck.cc
/// \brief Certificate checking: parse the document's forms, rebuild the
/// induced model from the current spec via the bridge, walk the (L)
/// inclusions, scan the invariant (G1–G3).
///
/// Tables are local flat structs — deliberately not the loop's
/// classifier type: the checker owns every step of its verdict.

#include "hsc/surface/certcheck.hh"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <map>
#include <ostream>
#include <string>

namespace hsc::surface {

namespace {

/// A parsed class table: complete action, one dead index or -1.
struct table {
  std::int32_t k = 0;
  std::int32_t dead = -1;
  std::int32_t nl = 0;            ///< the declaring leaf's alphabet
  std::vector<std::int32_t> act;  ///< k x nl
};

std::int32_t as_int(const datum& d, std::ostream& out, bool& ok) {
  std::int32_t v = 0;
  if (d.is_atom()) {
    const std::string& t = d.text();
    const auto r = std::from_chars(t.data(), t.data() + t.size(), v);
    if (r.ec == std::errc{} && r.ptr == t.data() + t.size()) return v;
  }
  out << "certcheck: malformed integer at line " << d.line() << '\n';
  ok = false;
  return 0;
}

}  // namespace

int certcheck(const spec& s, lia::expr_factory& ex, const expr_reader& reader,
              const std::vector<datum>& cert_forms, std::ostream& out) {
  int fails = 0;
  bool parse_ok = true;

  // --- gather the document's forms ----------------------------------------
  const std::vector<datum>* select = nullptr;
  std::map<std::string, std::vector<const datum*>> tables_raw;  // leaf -> (classifier ...)
  std::map<std::string, std::string> uses;                      // instance -> rep
  std::vector<const datum*> inv_raw;
  for (const datum& f : cert_forms) {
    if (f.head() == "certificate") {
      if (f.items().size() == 2 && f.items()[1].head() == "select") {
        select = &f.items()[1].items();
      } else {
        out << "certcheck: malformed certificate header\n";
        parse_ok = false;
      }
    } else if (f.head() == "classifier") {
      if (f.items().size() >= 4 && f.items()[1].is_atom()) {
        tables_raw[f.items()[1].text()].push_back(&f);
      } else {
        out << "certcheck: malformed classifier form\n";
        parse_ok = false;
      }
    } else if (f.head() == "use") {
      if (f.items().size() == 3) {
        uses[f.items()[1].text()] = f.items()[2].text();
      } else {
        out << "certcheck: malformed use form\n";
        parse_ok = false;
      }
    } else if (f.head() == "inv") {
      inv_raw.push_back(&f);
    } else {
      out << "certcheck: unknown form '" << f.head() << "' at line "
          << f.line() << '\n';
      parse_ok = false;
    }
  }
  if (!select) {
    out << "certcheck: no (certificate (select …)) header\n";
    return fails + 1;
  }
  if (!parse_ok) ++fails;

  // --- rebuild the induced model from the current spec --------------------
  std::vector<datum> atoms(select->begin() + 1, select->end());
  const cegar_bridge b = build_cegar_model(s, ex, reader, atoms);
  const cegar::model& m = b.model;
  const std::size_t n = m.leaves.size();
  std::map<std::string, std::int32_t> leaf_index;
  for (std::size_t i = 0; i < n; ++i)
    leaf_index[m.leaf_names[i]] = static_cast<std::int32_t>(i);

  // --- resolve one table per non-property leaf ----------------------------
  std::vector<const table*> of_leaf(n, nullptr);
  std::map<std::string, table> parsed;
  for (const auto& [name, forms] : tables_raw) {
    if (forms.size() > 1) {
      out << "certcheck: duplicate classifier for '" << name << "'\n";
      ++fails;
    }
    const datum& f = *forms.front();
    const auto it = leaf_index.find(name);
    if (it == leaf_index.end()) {
      out << "certcheck: classifier for unknown leaf '" << name << "'\n";
      ++fails;
      continue;
    }
    const std::int32_t nl = m.leaves[static_cast<std::size_t>(it->second)]
                                .n_letters;
    table t;
    bool ok = true;
    t.nl = nl;
    t.k = as_int(f.items()[2], out, ok);
    t.dead = as_int(f.items()[3], out, ok);
    if (!ok || t.k < 1 || t.dead < -1 || t.dead >= t.k) {
      out << "certcheck: classifier '" << name << "' malformed header\n";
      ++fails;
      continue;
    }
    t.act.assign(static_cast<std::size_t>(t.k) * nl, -1);
    for (std::size_t i = 4; i < f.items().size(); ++i) {
      const datum& a = f.items()[i];
      if (a.head() != "a" || a.items().size() != 4) {
        out << "certcheck: classifier '" << name << "' malformed action\n";
        ok = false;
        break;
      }
      const std::int32_t c = as_int(a.items()[1], out, ok);
      const std::int32_t l = as_int(a.items()[2], out, ok);
      const std::int32_t d = as_int(a.items()[3], out, ok);
      if (!ok || c < 0 || c >= t.k || l < 0 || l >= nl || d < 0 || d >= t.k) {
        out << "certcheck: classifier '" << name << "' action out of range\n";
        ok = false;
        break;
      }
      t.act[static_cast<std::size_t>(c) * nl + l] = d;
    }
    if (ok && std::find(t.act.begin(), t.act.end(), -1) != t.act.end()) {
      out << "certcheck: classifier '" << name << "' action table incomplete\n";
      ok = false;
    }
    if (!ok) {
      ++fails;
      continue;
    }
    parsed[name] = std::move(t);
  }
  for (const auto& [inst, rep] : uses) {
    const auto ti = parsed.find(rep);
    const auto li = leaf_index.find(inst);
    if (ti == parsed.end() || li == leaf_index.end()) {
      out << "certcheck: use '" << inst << "' -> '" << rep
          << "' does not resolve\n";
      ++fails;
      continue;
    }
    if (ti->second.nl !=
        m.leaves[static_cast<std::size_t>(li->second)].n_letters) {
      out << "FAIL (L) leaf '" << inst
          << "': aliased table has a different alphabet\n";
      ++fails;
      continue;
    }
    of_leaf[static_cast<std::size_t>(li->second)] = &ti->second;
  }
  for (const auto& [name, t] : parsed)
    of_leaf[static_cast<std::size_t>(leaf_index.at(name))] = &t;
  for (std::size_t i = 0; i < n; ++i) {
    if (m.is_prop(static_cast<std::int32_t>(i))) continue;
    if (!of_leaf[i]) {
      out << "FAIL (L) leaf '" << m.leaf_names[i] << "': no classifier\n";
      ++fails;
    }
  }

  // --- (L) one inclusion walk per distinct table --------------------------
  for (const auto& [name, t] : parsed) {
    const cegar::lts& leaf =
        m.leaves[static_cast<std::size_t>(leaf_index.at(name))];
    const std::int32_t nl = leaf.n_letters;
    std::vector<bool> seen(
        static_cast<std::size_t>(leaf.n_states) * t.k, false);
    std::vector<std::int64_t> queue;
    bool bad = false;
    auto push = [&](std::int32_t q, std::int32_t c) {
      if (c == t.dead) {
        bad = true;
        return;
      }
      const std::size_t id = static_cast<std::size_t>(q) * t.k + c;
      if (!seen[id]) {
        seen[id] = true;
        queue.push_back(static_cast<std::int64_t>(id));
      }
    };
    push(0, 0);
    for (std::size_t head = 0; head < queue.size() && !bad; ++head) {
      const std::int32_t q = static_cast<std::int32_t>(queue[head] / t.k);
      const std::int32_t c = static_cast<std::int32_t>(queue[head] % t.k);
      for (std::int32_t a = 0; a < nl && !bad; ++a) {
        const std::int32_t q2 = leaf.step(q, static_cast<cegar::letter>(a));
        if (q2 < 0) continue;
        push(q2, t.act[static_cast<std::size_t>(c) * nl + a]);
      }
    }
    if (bad) {
      out << "FAIL (L) leaf '" << name << "': a leaf trace is classified dead\n";
      ++fails;
    } else {
      out << "ok   (L) leaf '" << name << "'\n";
    }
  }

  // --- decode inv entries into abstract states ----------------------------
  std::map<std::vector<std::int32_t>, std::int32_t> mstate_of;
  for (std::size_t t = 0; t < m.monitor_values.size(); ++t)
    mstate_of[m.monitor_values[t]] = static_cast<std::int32_t>(t);
  std::map<std::vector<std::int32_t>, bool> inv;  // (m, x_1..x_n) -> present
  bool inv_ok = true;
  for (const datum* f : inv_raw) {
    std::vector<std::int32_t> vals(n, -1);
    bool ok = true;
    for (std::size_t i = 1; i < f->items().size(); ++i) {
      const datum& pair = (*f).items()[i];
      if (!pair.is_list() || pair.items().size() != 2 ||
          !pair.items()[0].is_atom()) {
        out << "certcheck: malformed inv pair at line " << pair.line() << '\n';
        ok = false;
        break;
      }
      const auto li = leaf_index.find(pair.items()[0].text());
      if (li == leaf_index.end()) {
        out << "certcheck: inv names unknown leaf '" << pair.items()[0].text()
            << "'\n";
        ok = false;
        break;
      }
      vals[static_cast<std::size_t>(li->second)] =
          as_int(pair.items()[1], out, ok);
    }
    if (!ok) {
      inv_ok = false;
      continue;
    }
    std::vector<std::int32_t> st(n + 1, 0);
    std::vector<std::int32_t> pvals;
    for (std::int32_t p : m.prop_leaves)
      pvals.push_back(vals[static_cast<std::size_t>(p)]);
    const auto ms = mstate_of.find(pvals);
    if (ms == mstate_of.end()) {
      out << "certcheck: inv entry names an unreachable property tuple\n";
      inv_ok = false;
      continue;
    }
    st[0] = ms->second;
    bool in_range = true;
    for (std::size_t i = 0; i < n; ++i) {
      if (m.is_prop(static_cast<std::int32_t>(i))) continue;
      const std::int32_t c = vals[i];
      if (!of_leaf[i] || c < 0 || c >= of_leaf[i]->k) {
        out << "certcheck: inv class out of range for '" << m.leaf_names[i]
            << "'\n";
        in_range = false;
        break;
      }
      st[i + 1] = c;
    }
    if (!in_range) {
      inv_ok = false;
      continue;
    }
    inv[st] = true;
  }
  if (!inv_ok) ++fails;

  // --- (G1) the initial abstract state ------------------------------------
  const std::vector<std::int32_t> init(n + 1, 0);
  if (inv.contains(init)) {
    out << "ok   (G1) initial state in inv\n";
  } else {
    out << "FAIL (G1) initial state not in inv\n";
    ++fails;
  }

  // --- (G2) closure, (G3) no bad ------------------------------------------
  bool g2 = true, g3 = true;
  for (const auto& [st, _] : inv) {
    if (m.mon.bad[static_cast<std::size_t>(st[0])]) g3 = false;
    for (std::int32_t e = 0; e < m.mon.n_events; ++e) {
      std::vector<std::int32_t> nxt = st;
      nxt[0] = m.mon.step(st[0], e);
      if (nxt[0] < 0) continue;  // blocked by a property leaf: closed
      bool blocked = false;
      for (const auto& [l, a] : m.events[e].support) {
        if (m.is_prop(l)) continue;
        const table& t = *of_leaf[static_cast<std::size_t>(l)];
        const std::int32_t c =
            t.act[static_cast<std::size_t>(st[l + 1]) *
                      m.leaves[static_cast<std::size_t>(l)].n_letters +
                  a];
        if (c == t.dead) {
          blocked = true;  // blocked by a dead class: closed
          break;
        }
        nxt[static_cast<std::size_t>(l) + 1] = c;
      }
      if (blocked) continue;
      if (!inv.contains(nxt)) {
        g2 = false;
        break;
      }
    }
    if (!g2) break;
  }
  out << (g2 ? "ok   (G2) inv closed under every event\n"
             : "FAIL (G2) inv not closed\n");
  out << (g3 ? "ok   (G3) no inv state is bad\n"
             : "FAIL (G3) a bad state is in inv\n");
  if (!g2) ++fails;
  if (!g3) ++fails;

  out << (fails == 0 ? "certcheck: PASS\n"
                     : "certcheck: FAIL (" + std::to_string(fails) +
                           " obligations)\n");
  return fails;
}

}  // namespace hsc::surface
