/// \file id_table.hh
/// \brief The unique table's content-addressed direction: a set of ids.
///
/// A flat array of ids, power-of-two sized, open addressing with linear
/// probing, 0 = empty. It stores ids and nothing else, and delegates hashing
/// and comparison to callables supplied per call — so it is not templated on
/// the value type, and one instantiation serves every intern<T>.
///
/// No tombstones: entries are removed only by the wholesale rebuild that
/// collection performs. That is what lets the storage be a bare array.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace hsc::mem {

/// \brief An open-addressed set of non-zero ids.
template <typename Id = std::uint32_t>
class id_table {
 public:
  using id_type = Id;

  /// Outcome of a probe: either a hit, or the slot a new id belongs in.
  struct probe_result {
    id_type id;      ///< the id found, or 0 on a miss
    std::size_t slot;  ///< where to place() a new id, valid only on a miss
    std::size_t probes;  ///< slots examined, for the meter

    [[nodiscard]] bool hit() const noexcept { return id != 0; }
  };

  /// \param initial_buckets rounded up to a power of two, at least 8.
  explicit id_table(std::size_t initial_buckets = 4096)
      : slots_(round_up(initial_buckets), id_type{0}) {}

  /// \brief Look for the id whose value \p eq accepts.
  ///
  /// \p eq is called with a stored id and answers whether it denotes the
  /// value being sought. On a miss the returned slot is where place() must
  /// put the new id; nothing may grow the table in between.
  template <typename Eq>
  [[nodiscard]] probe_result probe(std::size_t hash, Eq&& eq) const {
    const std::size_t mask = slots_.size() - 1;
    std::size_t slot = hash & mask;
    std::size_t steps = 1;
    for (;; ++steps) {
      const id_type id = slots_[slot];
      if (id == 0) return {id_type{0}, slot, steps};
      if (eq(id)) return {id, slot, steps};
      slot = (slot + 1) & mask;
    }
  }

  /// Commit \p id into the slot a preceding probe() reported as free.
  void place(std::size_t slot, id_type id) {
    assert(id != 0 && "id 0 is the empty marker");
    assert(slots_[slot] == 0 && "slot no longer free: the table grew");
    slots_[slot] = id;
    ++size_;
  }

  /// Whether the table should grow before the next insertion.
  [[nodiscard]] bool overloaded() const noexcept {
    return size_ * 10 >= slots_.size() * 7;  // load factor 0.7
  }

  /// \brief Rebuild at (at least) \p buckets slots, keeping every id.
  ///
  /// \p hash_of gives the hash of the value an id denotes.
  template <typename HashOf>
  void rehash(std::size_t buckets, HashOf&& hash_of) {
    std::vector<id_type> old;
    old.swap(slots_);
    slots_.assign(round_up(buckets < size_ * 2 ? size_ * 2 : buckets),
                  id_type{0});
    size_ = 0;
    for (const id_type id : old) {
      if (id != 0) insert_fresh(id, hash_of(id));
    }
  }

  /// \brief Replace the contents with \p ids, sized for them.
  ///
  /// The sweep's swap-not-erase path: survivors are inserted into a table
  /// sized for the survivors, and the old storage is released.
  template <typename HashOf>
  void rebuild(const std::vector<id_type>& ids, HashOf&& hash_of) {
    std::vector<id_type> fresh(round_up(ids.size() * 2 + 8), id_type{0});
    slots_.swap(fresh);
    fresh = std::vector<id_type>{};  // release the old storage now
    size_ = 0;
    for (const id_type id : ids) insert_fresh(id, hash_of(id));
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t bucket_count() const noexcept {
    return slots_.size();
  }

 private:
  /// Insert an id known to be absent, into a table known to have room.
  void insert_fresh(id_type id, std::size_t hash) {
    const std::size_t mask = slots_.size() - 1;
    std::size_t slot = hash & mask;
    while (slots_[slot] != 0) slot = (slot + 1) & mask;
    slots_[slot] = id;
    ++size_;
  }

  static std::size_t round_up(std::size_t n) noexcept {
    std::size_t p = 8;
    while (p < n) p <<= 1;
    return p;
  }

  std::vector<id_type> slots_;
  std::size_t size_ = 0;
};

}  // namespace hsc::mem
