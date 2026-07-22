/// \file handle.hh
/// \brief Typed references into a unique table.
///
/// Three of them, for three jobs:
///   weak<T>        a bare id. What edges hold. No refcount traffic.
///   strong<T>      a root: refcounted, keeps its value alive.
///   certificate<T> id plus generation: survives a collection knowingly.
///
/// The weak/strong split is libDDD's GDDD/DDD distinction, kept because it is
/// right: refcounting on every copy (libsdd's `mem::ptr`) puts real traffic on
/// the hottest path in the library, and edges outnumber roots by orders of
/// magnitude.
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "hsc/mem/intern.hh"
#include "hsc/util/hash.hh"

namespace hsc::mem {

/// \brief A bare id in a typed wrapper. Trivially copyable; 0 is absence.
///
/// Resolution needs the table, which is reached as an explicit context — a
/// weak handle deliberately does not know where it came from.
template <typename T, typename Id = std::uint32_t>
class weak {
 public:
  using value_type = T;
  using id_type = Id;

  constexpr weak() noexcept = default;
  constexpr explicit weak(id_type id) noexcept : id_(id) {}

  [[nodiscard]] constexpr id_type id() const noexcept { return id_; }
  constexpr explicit operator bool() const noexcept { return id_ != 0; }

  [[nodiscard]] const T& operator()(const intern<T, Id>& table) const noexcept {
    return table[id_];
  }

  friend constexpr bool operator==(weak, weak) noexcept = default;
  friend constexpr auto operator<=>(weak, weak) noexcept = default;

  [[nodiscard]] std::size_t hash() const noexcept {
    return util::hash_value(id_);
  }

 private:
  id_type id_ = 0;
};

/// \brief A referenced id: what a root is.
///
/// Two words. Never put one on an edge — that is what weak is for.
template <typename T, typename Id = std::uint32_t>
class strong {
 public:
  using value_type = T;
  using id_type = Id;
  using table_type = intern<T, Id>;

  constexpr strong() noexcept = default;

  strong(table_type& table, weak<T, Id> w) noexcept
      : table_(&table), id_(w.id()) {
    if (id_ != 0) table_->ref(id_);
  }

  strong(const strong& other) noexcept : table_(other.table_), id_(other.id_) {
    if (id_ != 0) table_->ref(id_);
  }

  strong(strong&& other) noexcept
      : table_(other.table_), id_(std::exchange(other.id_, id_type{0})) {}

  strong& operator=(strong other) noexcept {  // copy-and-swap
    swap(*this, other);
    return *this;
  }

  ~strong() {
    if (id_ != 0) table_->deref(id_);
  }

  friend void swap(strong& a, strong& b) noexcept {
    std::swap(a.table_, b.table_);
    std::swap(a.id_, b.id_);
  }

  [[nodiscard]] id_type id() const noexcept { return id_; }
  [[nodiscard]] weak<T, Id> ref() const noexcept { return weak<T, Id>(id_); }
  explicit operator bool() const noexcept { return id_ != 0; }

  [[nodiscard]] const T& operator*() const noexcept { return (*table_)[id_]; }
  [[nodiscard]] const T* operator->() const noexcept { return &**this; }

  friend bool operator==(const strong& a, const strong& b) noexcept {
    return a.id_ == b.id_ && a.table_ == b.table_;
  }

  [[nodiscard]] std::size_t hash() const noexcept {
    return util::hash_value(id_);
  }

 private:
  table_type* table_ = nullptr;
  id_type id_ = 0;
};

/// \brief An id together with the generation it was issued in.
///
/// The only thing that can honestly cite an id across a collection. Cheap to
/// make, never consulted on the hot path; this is what lets a cache entry or
/// a retention policy survive a collection instead of being wiped with
/// everything else.
template <typename T, typename Id = std::uint32_t>
class certificate {
 public:
  using value_type = T;
  using id_type = Id;
  using generation_type = typename intern<T, Id>::generation_type;

  constexpr certificate() noexcept = default;

  certificate(const intern<T, Id>& table, weak<T, Id> w) noexcept
      : id_(w.id()), generation_(table.generation(w.id())) {}

  [[nodiscard]] id_type id() const noexcept { return id_; }
  [[nodiscard]] generation_type generation() const noexcept {
    return generation_;
  }

  /// Whether the id still denotes what this certificate was made for.
  [[nodiscard]] bool valid(const intern<T, Id>& table) const noexcept {
    return table.valid(id_, generation_);
  }

  /// The handle, checked. Absent if the value did not survive.
  [[nodiscard]] weak<T, Id> get(const intern<T, Id>& table) const noexcept {
    return valid(table) ? weak<T, Id>(id_) : weak<T, Id>();
  }

  friend constexpr bool operator==(certificate, certificate) noexcept = default;

  [[nodiscard]] std::size_t hash() const noexcept {
    return util::hash_all(id_, generation_);
  }

 private:
  id_type id_ = 0;
  generation_type generation_ = 0;
};

}  // namespace hsc::mem
