/// \file store.hh
/// \brief The state store: fixed-arity rows, deduplicated, open-addressed.
///
/// Insert-if-absent is the one operation — and the one linearization point
/// a parallel search would shard by hash. Ids are dense, allocation order.
/// Views are invalidated by the next insert: copy a state out before
/// expanding it.
#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

#include "hsc/util/hash.hh"
#include "hsc/xpl/state.hh"

namespace hsc::xpl {

class state_store {
 public:
  explicit state_store(std::size_t arity)
      : arity_(arity), table_(kInitialBuckets, 0) {}

  /// Intern one state (must have `arity()` values). Returns (id, fresh).
  std::pair<state_id, bool> intern(state_view s) {
    assert(s.size() == arity_);
    const std::size_t mask = table_.size() - 1;
    std::size_t idx = hash(s) & mask;
    while (table_[idx] != 0) {
      const state_id id = table_[idx] - 1;
      if (std::equal(s.begin(), s.end(), row(id))) return {id, false};
      idx = (idx + 1) & mask;
    }
    const state_id id = static_cast<state_id>(count_);
    rows_.insert(rows_.end(), s.begin(), s.end());
    table_[idx] = id + 1;
    ++count_;
    if (count_ * 4 >= table_.size() * 3) grow();
    return {id, true};
  }

  [[nodiscard]] state_view operator[](state_id id) const {
    return {row(id), arity_};
  }
  [[nodiscard]] std::size_t size() const { return count_; }
  [[nodiscard]] std::size_t arity() const { return arity_; }

 private:
  static constexpr std::size_t kInitialBuckets = 1u << 10;  // power of two

  [[nodiscard]] const value* row(state_id id) const {
    return rows_.data() + static_cast<std::size_t>(id) * arity_;
  }
  [[nodiscard]] std::size_t hash(state_view s) const {
    std::size_t seed = 0;
    util::hash_range(seed, s.begin(), s.end());
    return seed;
  }

  void grow() {
    std::vector<state_id> bigger(table_.size() * 2, 0);
    const std::size_t mask = bigger.size() - 1;
    for (const state_id slot : table_) {
      if (slot == 0) continue;
      std::size_t idx = hash({row(slot - 1), arity_}) & mask;
      while (bigger[idx] != 0) idx = (idx + 1) & mask;
      bigger[idx] = slot;
    }
    table_ = std::move(bigger);
  }

  std::size_t arity_;
  std::vector<value> rows_;      ///< `count_` rows of `arity_` values
  std::vector<state_id> table_;  ///< open addressing; id+1, 0 = empty
  std::size_t count_ = 0;
};

}  // namespace hsc::xpl
