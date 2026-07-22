/// \file intern.hh
/// \brief The unique table. One mechanism, instantiated per interned kind.
///
/// Design descends from libDDD's `ddd/UniqueTableId.hh` — id handles, an
/// index vector from id to object, a free list, sparse refcounts as mark &
/// sweep roots — with the known fixes applied from the start: probing by
/// view instead of the sentinel-id slot, an explicit free list instead of
/// pointers punned into ids, iterative marking, rebuild-not-erase on sweep,
/// and generation tags. See `algorithm.md` §1 for the whole design.
#pragma once

#include <sparsehash/sparsetable>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

#include "hsc/mem/id_table.hh"
#include "hsc/mem/stats.hh"
#include "hsc/util/hash.hh"

namespace hsc::mem {

/// \brief What a probe view must provide to stand in for a value of \p T.
///
/// A view describes a value that does not exist yet, so that a lookup that
/// hits allocates nothing. Contract: `hash()` must agree with the hash of the
/// value `construct()` would build, and `equals(x)` must agree with comparing
/// that value to `x`.
template <typename V, typename T>
concept interning_view = requires(const V& v, const T& stored, void* mem) {
  { v.hash() } -> std::convertible_to<std::size_t>;
  { v.equals(stored) } -> std::convertible_to<bool>;
  { v.extra_bytes() } -> std::convertible_to<std::size_t>;
  { v.construct(mem) } -> std::same_as<T*>;
};

/// \brief The view of an already-built value: the fixed-size case.
template <typename T>
class value_view {
 public:
  explicit value_view(const T& v) noexcept : value_(v) {}

  [[nodiscard]] std::size_t hash() const { return util::hash_value(value_); }
  [[nodiscard]] bool equals(const T& stored) const { return value_ == stored; }
  [[nodiscard]] std::size_t extra_bytes() const noexcept { return 0; }
  T* construct(void* mem) const { return new (mem) T(value_); }

 private:
  const T& value_;
};

/// \brief A unique table over values of type \p T, addressed by \p Id.
///
/// Not copyable and not movable: ids and the addresses behind them are handed
/// out and must stay where they are.
template <typename T, typename Id = std::uint32_t>
class intern {
  static_assert(std::is_unsigned_v<Id>, "ids are unsigned; 0 means absence");
  static_assert(alignof(T) <= alignof(std::max_align_t),
                "over-aligned interned types need an aligned allocation path");

 public:
  using value_type = T;
  using id_type = Id;
  using generation_type = std::uint32_t;

  explicit intern(std::size_t initial_capacity = 4096)
      : table_(initial_capacity) {
    // id 0 is absence: reserve its slot so no value ever gets it.
    index_.push_back(nullptr);
    generations_.push_back(0);
    marks_.push_back(false);
    index_.reserve(initial_capacity);
  }

  ~intern() {
    for (T* p : index_) {
      if (p) destroy(p);
    }
  }

  intern(const intern&) = delete;
  intern& operator=(const intern&) = delete;

  /// \brief The id of the value \p view describes, interning it if new.
  ///
  /// Allocates nothing when the value is already present, which is the
  /// common case.
  template <typename V>
    requires interning_view<V, T>
  id_type get(const V& view) {
    ++stats_.lookups;
    if (table_.overloaded()) grow_table();

    const std::size_t h = view.hash();
    const auto probe = table_.probe(
        h, [&](id_type id) { return view.equals(*index_[id]); });
    stats_.probes += probe.probes;

    if (probe.hit()) {
      ++stats_.hits;
      return probe.id;
    }

    ++stats_.misses;
    char* mem = new char[sizeof(T) + view.extra_bytes()];
    T* value = nullptr;
    try {
      value = view.construct(mem);
    } catch (...) {
      delete[] mem;
      throw;
    }
    assert(util::hash_value(*value) == h &&
           "view.hash() disagrees with the hash of the value it builds");

    const id_type id = allocate_id();
    index_[id] = value;
    table_.place(probe.slot, id);
    if (table_.size() > stats_.peak) stats_.peak = table_.size();
    return id;
  }

  /// Convenience for fixed-size values already in hand.
  id_type get(const T& value)
    requires std::copy_constructible<T>
  {
    return get(value_view<T>(value));
  }

  /// \name Resolution
  ///@{
  [[nodiscard]] const T& operator[](id_type id) const noexcept {
    assert(id != 0 && id < index_.size() && index_[id] != nullptr);
    return *index_[id];
  }
  [[nodiscard]] const T* resolve(id_type id) const noexcept {
    return id < index_.size() ? index_[id] : nullptr;
  }
  ///@}

  /// \name Certificates
  ///
  /// A certificate is a pair (id, generation). Freeing an id bumps its
  /// generation, so a holder that outlives a collection can tell whether the
  /// id still denotes what it meant. Nothing on the hot path consults these.
  ///@{
  [[nodiscard]] generation_type generation(id_type id) const noexcept {
    return id < generations_.size() ? generations_[id] : 0;
  }
  [[nodiscard]] bool valid(id_type id, generation_type gen) const noexcept {
    return id != 0 && id < index_.size() && index_[id] != nullptr &&
           generations_[id] == gen;
  }
  ///@}

  /// \name Roots
  ///
  /// Referenced ids are the roots of collection. Refcounts are stored
  /// sparsely because the referenced set is a vanishing fraction of the live
  /// set: roots are variables in user code, not edges.
  ///@{
  void ref(id_type id) {
    assert(id != 0);
    if (refs_.size() <= id) refs_.resize((3 * std::size_t{id}) / 2 + 8);
    refs_.set(id, refs_.test(id) ? refs_.get(id) + 1 : 1);
  }

  void deref(id_type id) {
    assert(id != 0 && refs_.test(id) && "deref of an unreferenced id");
    const auto count = refs_.get(id);
    if (count == 1) {
      refs_.erase(id);  // zero is stored as absence
    } else {
      refs_.set(id, count - 1);
    }
  }

  [[nodiscard]] std::uint32_t ref_count(id_type id) const noexcept {
    return (id < refs_.size() && refs_.test(id)) ? refs_.get(id) : 0;
  }
  ///@}

  /// \brief Mark and sweep from the referenced ids.
  ///
  /// \p mark_children is called as `mark_children(value, push)` and must call
  /// `push(id)` for every id *of this table* the value cites. It is supplied
  /// by the owner rather than demanded of \p T because a value typically also
  /// cites ids in other tables, which only the owner can reach.
  template <typename MarkChildren>
  void collect(MarkChildren&& mark_children) {
    std::vector<id_type> work;
    for (auto it = refs_.nonempty_begin(); it != refs_.nonempty_end(); ++it) {
      work.push_back(static_cast<id_type>(refs_.get_pos(it)));
    }

    const auto push = [&](id_type id) {
      if (id != 0 && !marks_[id]) work.push_back(id);
    };
    while (!work.empty()) {  // iterative: diagram spines are deep
      const id_type id = work.back();
      work.pop_back();
      if (marks_[id]) continue;
      marks_[id] = true;
      mark_children(*index_[id], push);
    }

    std::vector<id_type> survivors;
    survivors.reserve(table_.size());
    for (id_type id = 1; id < index_.size(); ++id) {
      T* value = index_[id];
      if (value == nullptr) continue;  // already free
      if (marks_[id]) {
        marks_[id] = false;
        survivors.push_back(id);
      } else {
        destroy(value);
        index_[id] = nullptr;
        free_.push_back(id);
        ++generations_[id];  // any certificate citing this id is now stale
        ++stats_.collected;
        ++stats_.generation_bumps;
      }
    }
    table_.rebuild(survivors, hash_of());
    ++stats_.collections;
  }

  /// Collection for values that cite no id of this table.
  void collect() {
    collect([](const T&, auto&&) {});
  }

  [[nodiscard]] std::size_t size() const noexcept { return table_.size(); }

  [[nodiscard]] const intern_statistics& stats() const noexcept {
    stats_.size = table_.size();
    stats_.capacity = index_.size() - 1;
    stats_.buckets = table_.bucket_count();
    return stats_;
  }

 private:
  static void destroy(T* p) noexcept {
    p->~T();
    delete[] reinterpret_cast<char*>(p);  // matches new char[] in get()
  }

  /// The probe table stores ids only, so growth asks us for the hash behind
  /// each one.
  auto hash_of() const {
    return [this](id_type id) { return util::hash_value(*index_[id]); };
  }

  void grow_table() {
    table_.rehash(table_.bucket_count() * 2, hash_of());
    ++stats_.rehashes;
  }

  id_type allocate_id() {
    if (!free_.empty()) {
      const id_type id = free_.back();
      free_.pop_back();
      marks_[id] = false;
      ++stats_.recycled;
      return id;
    }
    if (index_.size() > std::numeric_limits<id_type>::max()) {
      throw std::overflow_error("hsc::mem::intern: id space exhausted");
    }
    const id_type id = static_cast<id_type>(index_.size());
    index_.push_back(nullptr);
    generations_.push_back(0);
    marks_.push_back(false);
    return id;
  }

  std::vector<T*> index_;                    ///< id -> value
  id_table<Id> table_;                       ///< value -> id
  std::vector<generation_type> generations_; ///< id reuse counters
  std::vector<bool> marks_;                  ///< mark & sweep bitset
  std::vector<id_type> free_;                ///< free list, explicit
  google::sparsetable<std::uint32_t> refs_;  ///< refcounts of roots only
  mutable intern_statistics stats_;
};

}  // namespace hsc::mem
