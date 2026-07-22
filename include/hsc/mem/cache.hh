/// \file cache.hh
/// \brief The bounded operation cache.
///
/// Adapted from libsdd — `sdd/mem/cache.hh`, `cache_entry.hh`, `lru_list.hh`,
/// `hash_table.hh`, Copyright (c) 2012-2015 Alexandre Hamez, BSD-2 — and
/// relicensed here under the GPL with attribution kept. Deltas from the
/// original: the LRU order is intrusive rather than a std::list (no malloc
/// per miss), insertion re-probes after evaluation (re-entrancy), eviction
/// batches are a knob, admission is a concept plus a runtime filter rather
/// than a variadic compile-time chain, and the meters are extended.
///
/// See `algorithm.md` §4.
#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "hsc/mem/stats.hh"

namespace hsc::mem {

/// An operation that answers whether it is worth caching at all.
template <typename Op>
concept self_filtering = requires(const Op& op) {
  { op.cacheable() } -> std::convertible_to<bool>;
};

/// \brief A fixed-capacity, LRU-evicting memo over operations.
///
/// \tparam Context what an operation needs to evaluate: the tables and caches
///         it may reach. Held by reference — contexts are explicit, there are
///         no singletons.
/// \tparam Operation the key *and* the computation: it must provide
///         `hash()`, `operator==` and `result_type operator()(Context&)`.
template <typename Context, typename Operation>
class cache {
 public:
  using context_type = Context;
  using operation_type = Operation;
  using result_type = std::invoke_result_t<Operation&, Context&>;
  using filter_type = std::function<bool(const Operation&)>;

  static_assert(!std::is_void_v<result_type>,
                "a cached operation must return a value to cache");

  /// \param context the evaluation context, which must outlive this cache.
  /// \param capacity how many results are kept. All memory for them is
  ///        taken now; the cache never grows and never rehashes.
  /// \param evict_batch how many entries one eviction round drops.
  cache(Context& context, std::size_t capacity, std::size_t evict_batch = 1)
      : cxt_(context),
        capacity_(capacity ? capacity : 1),
        evict_batch_(evict_batch ? evict_batch : 1),
        buckets_(round_up(static_cast<std::size_t>(
                     static_cast<double>(capacity_) / max_load_factor)),
                 nullptr),
        storage_(std::make_unique<std::byte[]>(capacity_ * sizeof(entry))) {
    free_.reserve(capacity_);
    for (std::size_t i = capacity_; i-- > 0;) free_.push_back(slot(i));
  }

  ~cache() { clear(); }

  cache(const cache&) = delete;
  cache& operator=(const cache&) = delete;

  /// \brief Evaluate \p op, from the memo when possible.
  result_type operator()(Operation op) {
    ++stats_.lookups;

    if (!admitted(op)) {
      ++stats_.filtered;
      return op(cxt_);
    }

    const std::size_t h = op.hash();
    if (entry* found = find(h, op)) {
      ++stats_.hits;
      touch(found);
      return found->result;
    }

    ++stats_.misses;
    result_type result = op(cxt_);  // may re-enter this cache

    // Re-probe: a nested evaluation may have inserted this very operation.
    if (entry* raced = find(h, op)) {
      touch(raced);
      return raced->result;
    }

    if (free_.empty()) evict();
    insert(h, std::move(op), std::move(result));
    return back_->result;
  }

  /// Drop every entry.
  void clear() noexcept {
    for (entry* e = front_; e != nullptr;) {
      entry* const next = e->newer;
      e->~entry();
      free_.push_back(e);
      e = next;
    }
    std::fill(buckets_.begin(), buckets_.end(), nullptr);
    front_ = back_ = nullptr;
    size_ = 0;
  }

  /// \brief Install a runtime admission filter; an empty filter admits all.
  ///
  /// The attachment point for retention policy. A filter must be a function
  /// of the operation alone: the same operation must always get the same
  /// answer, or a result computed under one regime is served under another.
  void set_filter(filter_type filter) { filter_ = std::move(filter); }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

  [[nodiscard]] const cache_statistics& stats() const noexcept {
    stats_.size = size_;
    stats_.capacity = capacity_;
    stats_.buckets = buckets_.size();
    return stats_;
  }

 private:
  static constexpr double max_load_factor = 0.85;

  struct entry {
    entry* bucket_next;
    entry* older;  ///< toward the least recently used end
    entry* newer;
    std::size_t hash;
    const Operation operation;
    const result_type result;

    entry(std::size_t h, Operation&& op, result_type&& res)
        : bucket_next(nullptr),
          older(nullptr),
          newer(nullptr),
          hash(h),
          operation(std::move(op)),
          result(std::move(res)) {}
  };

  [[nodiscard]] bool admitted(const Operation& op) const {
    if constexpr (self_filtering<Operation>) {
      if (!op.cacheable()) return false;
    }
    return !filter_ || filter_(op);
  }

  static_assert(alignof(entry) <= alignof(std::max_align_t),
                "over-aligned cache entries need an aligned pool");

  [[nodiscard]] entry* slot(std::size_t i) noexcept {
    return reinterpret_cast<entry*>(storage_.get() + i * sizeof(entry));
  }

  [[nodiscard]] entry* find(std::size_t h, const Operation& op) const {
    std::size_t probes = 0;
    for (entry* e = buckets_[h & (buckets_.size() - 1)]; e != nullptr;
         e = e->bucket_next) {
      ++probes;
      if (e->hash == h && e->operation == op) {
        stats_.probes += probes;
        return e;
      }
    }
    stats_.probes += probes;
    return nullptr;
  }

  void insert(std::size_t h, Operation&& op, result_type&& result) {
    assert(!free_.empty());
    entry* const e = free_.back();
    free_.pop_back();
    new (e) entry(h, std::move(op), std::move(result));

    entry*& head = buckets_[h & (buckets_.size() - 1)];
    e->bucket_next = head;
    head = e;

    link_as_newest(e);
    ++size_;
  }

  /// Drop the \c evict_batch_ least recently used entries (at least one).
  void evict() {
    const std::size_t target = std::min(evict_batch_, size_);
    for (std::size_t i = 0; i < target; ++i) {
      entry* const oldest = front_;
      assert(oldest != nullptr);
      unlink_from_bucket(oldest);
      unlink_from_lru(oldest);
      oldest->~entry();
      free_.push_back(oldest);
      --size_;
      ++stats_.evicted;
    }
    ++stats_.evictions;
  }

  void unlink_from_bucket(entry* e) noexcept {
    entry** link = &buckets_[e->hash & (buckets_.size() - 1)];
    while (*link != e) {
      assert(*link != nullptr);
      link = &(*link)->bucket_next;
    }
    *link = e->bucket_next;
  }

  void unlink_from_lru(entry* e) noexcept {
    (e->older ? e->older->newer : front_) = e->newer;
    (e->newer ? e->newer->older : back_) = e->older;
  }

  void link_as_newest(entry* e) noexcept {
    e->older = back_;
    e->newer = nullptr;
    (back_ ? back_->newer : front_) = e;
    back_ = e;
  }

  /// Move an entry to the most-recently-used end.
  void touch(entry* e) noexcept {
    if (e == back_) return;
    unlink_from_lru(e);
    link_as_newest(e);
  }

  static std::size_t round_up(std::size_t n) noexcept {
    std::size_t p = 8;
    while (p < n) p <<= 1;
    return p;
  }

  Context& cxt_;
  std::size_t capacity_;
  std::size_t evict_batch_;
  std::vector<entry*> buckets_;
  std::unique_ptr<std::byte[]> storage_;  ///< all entries, allocated once
  std::vector<entry*> free_;              ///< unused slots of storage_
  entry* front_ = nullptr;                ///< least recently used
  entry* back_ = nullptr;                 ///< most recently used
  std::size_t size_ = 0;
  filter_type filter_;
  mutable cache_statistics stats_;
};

}  // namespace hsc::mem
