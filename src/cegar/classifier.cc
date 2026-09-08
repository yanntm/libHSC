/// \file cegar/classifier.cc — canonical class tables: dead-closure,
/// Moore minimization, shortlex renaming, serialization.

#include "hsc/cegar/classifier.hh"

#include <cstdint>
#include <cstring>
#include <map>

namespace hsc::cegar {

std::int32_t raw_table::classify(const word& w) const {
  std::int32_t c = 0;
  for (letter a : w) c = act[static_cast<std::size_t>(c) * n_letters + a];
  return c;
}

std::int32_t classifier::classify(const word& w) const {
  std::int32_t c = 0;
  for (letter a : w) c = step(c, a);
  return c;
}

std::string classifier::serialize() const {
  std::string out;
  out.reserve(12 + act.size() * 4);
  auto put = [&out](std::int32_t v) {
    char b[4];
    std::memcpy(b, &v, 4);
    out.append(b, 4);
  };
  put(n_letters);
  put(k);
  put(dead);
  for (std::int32_t c : act) put(c);
  return out;
}

classifier chaos(std::int32_t n_letters) {
  classifier h;
  h.n_letters = n_letters;
  h.k = 1;
  h.dead = -1;
  h.act.assign(static_cast<std::size_t>(n_letters), 0);
  h.reps.assign(1, word{});
  return h;
}

classifier canonicalize(const raw_table& t) {
  const std::int32_t L = t.n_letters;
  // 1. Dead-closure: route every dead class to one absorbing sink. The
  // sink is virtual here: partition ids below use -1 for "dead".
  // 2. Moore refinement on (live, successors) starting from live/dead.
  std::vector<std::int32_t> part(t.k);  // class -> block id
  for (std::int32_t c = 0; c < t.k; ++c) part[c] = t.live[c] ? 0 : 1;
  bool has_dead = false;
  for (std::int32_t c = 0; c < t.k; ++c) has_dead |= !t.live[c];
  std::int32_t n_blocks = has_dead ? 2 : 1;
  for (;;) {
    // Signature of a class: its block, then successor blocks per letter
    // (dead classes need no signature — they are one absorbing block).
    std::map<std::vector<std::int32_t>, std::int32_t> sig_to_block;
    std::vector<std::int32_t> next(t.k);
    for (std::int32_t c = 0; c < t.k; ++c) {
      if (!t.live[c]) {
        next[c] = -1;
        continue;
      }
      std::vector<std::int32_t> sig;
      sig.reserve(static_cast<std::size_t>(L) + 1);
      sig.push_back(part[c]);
      for (std::int32_t a = 0; a < L; ++a)
        sig.push_back(part[t.act[static_cast<std::size_t>(c) * L + a]]);
      auto [it, fresh] = sig_to_block.emplace(
          std::move(sig), static_cast<std::int32_t>(sig_to_block.size()));
      next[c] = it->second;
    }
    std::int32_t live_blocks = static_cast<std::int32_t>(sig_to_block.size());
    std::int32_t total = live_blocks + (has_dead ? 1 : 0);
    // Re-express dead as one block id past the live ones.
    for (std::int32_t c = 0; c < t.k; ++c)
      if (next[c] < 0) next[c] = live_blocks;
    if (total == n_blocks) {
      part = std::move(next);
      break;
    }
    n_blocks = total;
    part = std::move(next);
  }
  const std::int32_t dead_block =
      has_dead ? n_blocks - 1 : -1;

  // 3. BFS from the block of ε, letters ascending: discovery order is
  // shortlex order of least access words; unreachable blocks drop out.
  // The dead block, if reached, is placed like any other block.
  std::vector<std::int32_t> order(static_cast<std::size_t>(n_blocks), -1);
  std::vector<std::int32_t> block_rep_class(static_cast<std::size_t>(n_blocks),
                                            -1);
  for (std::int32_t c = 0; c < t.k; ++c)
    if (block_rep_class[part[c]] < 0) block_rep_class[part[c]] = c;
  classifier h;
  h.n_letters = L;
  std::vector<std::int32_t> bfs;
  auto visit = [&](std::int32_t block, const word& via) {
    if (order[block] >= 0) return;
    order[block] = static_cast<std::int32_t>(bfs.size());
    bfs.push_back(block);
    h.reps.push_back(via);
  };
  visit(part[0], word{});
  for (std::size_t head = 0; head < bfs.size(); ++head) {
    std::int32_t block = bfs[head];
    if (block == dead_block) continue;  // absorbing: successors = itself
    std::int32_t c = block_rep_class[block];
    for (std::int32_t a = 0; a < L; ++a) {
      word via = h.reps[head];
      via.push_back(static_cast<letter>(a));
      visit(part[t.act[static_cast<std::size_t>(c) * L + a]], via);
    }
  }
  h.k = static_cast<std::int32_t>(bfs.size());
  h.dead = (dead_block >= 0 && order[dead_block] >= 0) ? order[dead_block] : -1;
  h.act.assign(static_cast<std::size_t>(h.k) * L, 0);
  for (std::int32_t n = 0; n < h.k; ++n) {
    std::int32_t block = bfs[n];
    for (std::int32_t a = 0; a < L; ++a)
      h.act[static_cast<std::size_t>(n) * L + a] =
          (block == dead_block)
              ? n
              : order[part[t.act[static_cast<std::size_t>(
                                     block_rep_class[block]) *
                                     L +
                                 a]]];
  }
  return h;
}

}  // namespace hsc::cegar
