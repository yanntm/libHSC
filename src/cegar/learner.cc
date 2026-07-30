/// \file cegar/learner.cc — L* with Rivest–Schapire handling; membership
/// by executing the leaf; publication dead-closes and canonicalizes.

#include "hsc/cegar/learner.hh"

#include <cstdio>
#include <cstdlib>

namespace hsc::cegar {

namespace {

[[noreturn]] void die(const char* what) {
  std::fprintf(stderr, "cegar learner invariant violated: %s\n", what);
  std::abort();
}

word concat(const word& u, const word& v) {
  word w = u;
  w.insert(w.end(), v.begin(), v.end());
  return w;
}

}  // namespace

learner::learner(const lts& leaf) : leaf_(&leaf) {
  S_.push_back({});
  E_.push_back({});
  close();
}

bool learner::member(const word& w) {
  auto [it, fresh] = memo_.try_emplace(w, false);
  if (fresh) it->second = leaf_->fire(w).has_value();
  return it->second;
}

std::vector<bool> learner::row(const word& u) {
  std::vector<bool> r;
  r.reserve(E_.size());
  for (const word& e : E_) r.push_back(member(concat(u, e)));
  return r;
}

void learner::close() {
  for (bool grew = true; grew;) {
    grew = false;
    for (std::size_t s = 0; s < S_.size() && !grew; ++s) {
      for (std::int32_t a = 0; a < leaf_->n_letters && !grew; ++a) {
        word ua = S_[s];
        ua.push_back(static_cast<letter>(a));
        std::vector<bool> r = row(ua);
        bool found = false;
        for (const word& s2 : S_)
          if (row(s2) == r) {
            found = true;
            break;
          }
        if (!found) {
          S_.push_back(std::move(ua));
          grew = true;
        }
      }
    }
  }
  fresh_ = false;
}

raw_table learner::table() {
  raw_table t;
  t.n_letters = leaf_->n_letters;
  t.k = static_cast<std::int32_t>(S_.size());
  t.act.assign(static_cast<std::size_t>(t.k) * t.n_letters, -1);
  t.live.resize(S_.size());
  std::vector<std::vector<bool>> rows;
  rows.reserve(S_.size());
  for (const word& s : S_) rows.push_back(row(s));
  for (std::size_t s = 0; s < S_.size(); ++s) {
    t.live[s] = member(S_[s]);
    for (std::int32_t a = 0; a < t.n_letters; ++a) {
      word ua = S_[s];
      ua.push_back(static_cast<letter>(a));
      std::vector<bool> r = row(ua);
      for (std::size_t s2 = 0; s2 < rows.size(); ++s2)
        if (rows[s2] == r) {
          t.act[s * t.n_letters + a] = static_cast<std::int32_t>(s2);
          break;
        }
      if (t.act[s * t.n_letters + a] < 0) die("table not closed");
    }
  }
  return t;
}

const classifier& learner::published() {
  if (!fresh_) {
    published_ = canonicalize(table());
    fresh_ = true;
  }
  return published_;
}

void learner::add_counterexample(const word& w) {
  raw_table t = table();
  // Publication dead-closes, so the published misclassification may sit
  // on a proper prefix as far as the raw table is concerned: locate the
  // shortest prefix the raw table misclassifies (one must exist).
  word bad;
  bool found = false;
  for (std::size_t len = 0; len <= w.size() && !found; ++len) {
    word p(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(len));
    if (t.live[t.classify(p)] != member(p)) {
      bad = std::move(p);
      found = true;
    }
  }
  if (!found) die("counterexample agrees with the raw table");
  // Rivest–Schapire: A(i) = member(rep(class of bad[..i)) . bad[i..));
  // A(0) = member(bad), A(n) = table's classification: they differ, so
  // a flip exists; binary search finds it, the suffix is the experiment.
  auto A = [&](std::size_t i) {
    word p(bad.begin(), bad.begin() + static_cast<std::ptrdiff_t>(i));
    word tail(bad.begin() + static_cast<std::ptrdiff_t>(i), bad.end());
    return member(concat(S_[t.classify(p)], tail));
  };
  std::size_t lo = 0, hi = bad.size();
  const bool a0 = A(0);
  if (a0 == A(bad.size())) die("no flip in counterexample");
  while (hi - lo > 1) {
    std::size_t mid = (lo + hi) / 2;
    (A(mid) == a0 ? lo : hi) = mid;
  }
  // Flip between positions lo and lo+1: the suffix past the read letter
  // distinguishes rep(class)·letter from the class it was mapped to.
  word exp(bad.begin() + static_cast<std::ptrdiff_t>(lo) + 1, bad.end());
  for (const word& e : E_)
    if (e == exp) die("experiment already present");
  E_.push_back(std::move(exp));
  std::size_t before = S_.size();
  fresh_ = false;
  close();
  if (S_.size() <= before) die("counterexample did not grow the table");
  ++n_cex_;
}

}  // namespace hsc::cegar
