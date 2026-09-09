/// \file bandwidth.cc
/// \brief RCM, Sloan and the random control (`hsc/order/bandwidth.hh`).
#include "hsc/order/bandwidth.hh"

#include <algorithm>
#include <limits>
#include <numeric>
#include <queue>
#include <random>

namespace hsc::order {

namespace {

using adjacency = std::vector<std::vector<int>>;

adjacency build(int n, std::span<const louvain::edge> edges) {
  adjacency adj(static_cast<std::size_t>(n));
  for (const louvain::edge& e : edges) {
    if (e.src == e.dest || e.src < 0 || e.dest < 0 || e.src >= n || e.dest >= n) continue;
    adj[static_cast<std::size_t>(e.src)].push_back(e.dest);
    adj[static_cast<std::size_t>(e.dest)].push_back(e.src);
  }
  for (auto& a : adj) {
    std::ranges::sort(a);
    a.erase(std::unique(a.begin(), a.end()), a.end());
  }
  return adj;
}

/// Breadth-first distances from \p s inside the component; -1 elsewhere.
std::vector<int> distances(const adjacency& adj, int s) {
  std::vector<int> d(adj.size(), -1);
  std::queue<int> q;
  d[static_cast<std::size_t>(s)] = 0;
  q.push(s);
  while (!q.empty()) {
    const int v = q.front();
    q.pop();
    for (int w : adj[static_cast<std::size_t>(v)]) {
      if (d[static_cast<std::size_t>(w)] < 0) {
        d[static_cast<std::size_t>(w)] = d[static_cast<std::size_t>(v)] + 1;
        q.push(w);
      }
    }
  }
  return d;
}

/// A pseudo-peripheral pair of the component of \p start (Gibbs–Poole–
/// Stockmeyer style): from a minimum-degree node, move to a minimum-degree
/// node of the last level while the eccentricity grows.
std::pair<int, int> peripheral(const adjacency& adj, int start) {
  int s = start;
  std::vector<int> d = distances(adj, s);
  int ecc = *std::ranges::max_element(d);
  for (int round = 0; round < 8; ++round) {
    int e = s;
    std::size_t best = std::numeric_limits<std::size_t>::max();
    for (std::size_t v = 0; v < adj.size(); ++v) {
      if (d[v] == ecc && adj[v].size() < best) {
        best = adj[v].size();
        e = static_cast<int>(v);
      }
    }
    const std::vector<int> de = distances(adj, e);
    const int ecc_e = *std::ranges::max_element(de);
    if (ecc_e <= ecc) return {s, e};
    s = e;
    d = de;
    ecc = ecc_e;
  }
  return {s, s};
}

}  // namespace

std::vector<std::uint32_t> rcm(int n, std::span<const louvain::edge> edges) {
  const adjacency adj = build(n, edges);
  std::vector<std::uint32_t> out;
  out.reserve(static_cast<std::size_t>(n));
  std::vector<bool> seen(static_cast<std::size_t>(n), false);
  for (int root = 0; root < n; ++root) {
    if (seen[static_cast<std::size_t>(root)]) continue;
    // the component's start: its pseudo-peripheral node, from a minimum-degree seed
    int seed = root;
    const std::vector<int> dr = distances(adj, root);
    for (std::size_t v = 0; v < adj.size(); ++v)
      if (dr[v] >= 0 && adj[v].size() < adj[static_cast<std::size_t>(seed)].size()) seed = static_cast<int>(v);
    const int s = peripheral(adj, seed).first;
    std::queue<int> q;
    q.push(s);
    seen[static_cast<std::size_t>(s)] = true;
    while (!q.empty()) {
      const int v = q.front();
      q.pop();
      out.push_back(static_cast<std::uint32_t>(v));
      std::vector<int> next;
      for (int w : adj[static_cast<std::size_t>(v)])
        if (!seen[static_cast<std::size_t>(w)]) {
          seen[static_cast<std::size_t>(w)] = true;
          next.push_back(w);
        }
      std::ranges::sort(next, [&](int a, int b) {
        return adj[static_cast<std::size_t>(a)].size() < adj[static_cast<std::size_t>(b)].size();
      });
      for (int w : next) q.push(w);
    }
  }
  std::ranges::reverse(out);
  return out;
}

std::vector<std::uint32_t> sloan(int n, std::span<const louvain::edge> edges, int w1, int w2) {
  const adjacency adj = build(n, edges);
  enum class st : std::uint8_t { inactive, preactive, active, postactive };
  std::vector<st> status(static_cast<std::size_t>(n), st::inactive);
  std::vector<long long> prio(static_cast<std::size_t>(n), 0);
  std::vector<std::uint32_t> out;
  out.reserve(static_cast<std::size_t>(n));
  for (int root = 0; root < n; ++root) {
    if (status[static_cast<std::size_t>(root)] != st::inactive) continue;
    int seed = root;
    const std::vector<int> dr = distances(adj, root);
    for (std::size_t v = 0; v < adj.size(); ++v)
      if (dr[v] >= 0 && adj[v].size() < adj[static_cast<std::size_t>(seed)].size()) seed = static_cast<int>(v);
    const auto [s, e] = peripheral(adj, seed);
    const std::vector<int> de = distances(adj, e);
    for (std::size_t v = 0; v < adj.size(); ++v)
      if (de[v] >= 0) prio[v] = static_cast<long long>(w1) * de[v] - static_cast<long long>(w2) * (static_cast<long long>(adj[v].size()) + 1);
    // a max-heap of (priority, node) with stale entries skipped
    std::priority_queue<std::pair<long long, int>> heap;
    const auto raise = [&](int v, long long by) {
      prio[static_cast<std::size_t>(v)] += by;
      if (status[static_cast<std::size_t>(v)] != st::postactive) heap.emplace(prio[static_cast<std::size_t>(v)], v);
    };
    status[static_cast<std::size_t>(s)] = st::preactive;
    heap.emplace(prio[static_cast<std::size_t>(s)], s);
    while (!heap.empty()) {
      const auto [p, v] = heap.top();
      heap.pop();
      if (status[static_cast<std::size_t>(v)] == st::postactive || p != prio[static_cast<std::size_t>(v)]) continue;
      if (status[static_cast<std::size_t>(v)] == st::preactive) {
        for (int w : adj[static_cast<std::size_t>(v)]) {
          if (status[static_cast<std::size_t>(w)] == st::inactive) status[static_cast<std::size_t>(w)] = st::preactive;
          raise(w, w2);
        }
      }
      status[static_cast<std::size_t>(v)] = st::postactive;
      out.push_back(static_cast<std::uint32_t>(v));
      for (int w : adj[static_cast<std::size_t>(v)]) {
        if (status[static_cast<std::size_t>(w)] != st::preactive) continue;
        status[static_cast<std::size_t>(w)] = st::active;
        raise(w, w2);
        for (int x : adj[static_cast<std::size_t>(w)]) {
          if (status[static_cast<std::size_t>(x)] == st::postactive) continue;
          if (status[static_cast<std::size_t>(x)] == st::inactive) status[static_cast<std::size_t>(x)] = st::preactive;
          raise(x, w2);
        }
      }
    }
  }
  return out;
}

std::vector<std::uint32_t> random_order(int n, std::uint64_t seed) {
  std::vector<std::uint32_t> out(static_cast<std::size_t>(n));
  std::iota(out.begin(), out.end(), 0u);
  std::mt19937_64 gen(seed);
  std::ranges::shuffle(out, gen);
  return out;
}

}  // namespace hsc::order
