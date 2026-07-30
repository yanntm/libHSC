/// \file cegar/certify.cc — the leaf-local inclusion walk.

#include "hsc/cegar/certify.hh"

#include <cstddef>
#include <utility>
#include <vector>

namespace hsc::cegar {

cert_result certify(const lts& l, const classifier& h) {
  const std::size_t total =
      static_cast<std::size_t>(l.n_states) * h.k;
  std::vector<bool> seen(total, false);
  // Predecessor per pair, to rebuild the counterexample word.
  std::vector<std::pair<std::int32_t, letter>> pred(total, {-1, 0});
  std::vector<std::int32_t> queue;
  auto id = [&](std::int32_t q, std::int32_t c) {
    return q * h.k + c;
  };
  auto access = [&](std::int32_t p) {
    word w;
    for (; pred[p].first >= 0; p = pred[p].first) w.push_back(pred[p].second);
    return word(w.rbegin(), w.rend());
  };
  seen[id(0, 0)] = true;
  queue.push_back(id(0, 0));
  for (std::size_t head = 0; head < queue.size(); ++head) {
    std::int32_t p = queue[head];
    std::int32_t q = p / h.k, c = p % h.k;
    for (std::int32_t a = 0; a < l.n_letters; ++a) {
      std::int32_t q2 = l.step(q, static_cast<letter>(a));
      if (q2 < 0) continue;
      std::int32_t c2 = h.step(c, static_cast<letter>(a));
      if (!h.live(c2)) {
        cert_result r;
        r.certified = false;
        r.counterexample = access(p);
        r.counterexample.push_back(static_cast<letter>(a));
        return r;
      }
      std::int32_t p2 = id(q2, c2);
      if (seen[p2]) continue;
      seen[p2] = true;
      pred[p2] = {p, static_cast<letter>(a)};
      queue.push_back(p2);
    }
  }
  return {true, {}};
}

}  // namespace hsc::cegar
