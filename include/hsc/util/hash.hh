/// \file hash.hh
/// \brief The hashing customization point and the sanctioned combiners.
///
/// Provenance: the integer mixers descend from libDDD's `ddd/hashfunc.hh`
/// (Wang's 32-bit hash, Knuth's multiplicative hash); the seed/combine shape
/// follows libsdd's `sdd/util/hash.hh` (Alexandre Hamez, BSD-2).
///
/// Hygiene rule, enforced everywhere in libHSC: composite hashes are built
/// with combine(), which is *not* commutative. XOR of member hashes is banned
/// — it makes every permutation of a composite collide, which for a decision
/// diagram means every reordering of a node's arcs lands in one bucket.
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

namespace hsc::util {

/// The 64-bit golden ratio; the odd multiplier used by the mixers.
inline constexpr std::uint64_t golden64 = 0x9E3779B97F4A7C15ull;

/// \brief Avalanche a 64-bit word (the splitmix64 finalizer).
///
/// Every input bit affects every output bit. Applied to identifiers, which
/// are dense small integers and therefore have no entropy in their high bits.
[[nodiscard]] constexpr std::uint64_t mix64(std::uint64_t z) noexcept {
  z += golden64;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

/// \brief Wang's 32-bit hash, kept from libDDD.
///
/// Cheaper than mix64 (shifts and adds, no multiply) and adequate when the
/// input is a small dense integer and the result feeds a 32-bit table.
[[nodiscard]] constexpr std::uint32_t mix32(std::uint32_t key) noexcept {
  key += ~(key << 15);
  key ^= (key >> 10);
  key += (key << 3);
  key ^= (key >> 6);
  key += ~(key << 11);
  key ^= (key >> 16);
  return key;
}

/// \brief Knuth's multiplicative hash, for values whose low bits are pure
/// alignment padding (machine addresses).
[[nodiscard]] constexpr std::uint64_t mix_ptr(std::uint64_t key) noexcept {
  return (key >> 3) * golden64;
}

/// \brief Fold \p value into \p seed. The only sanctioned combiner.
///
/// Non-commutative: the accumulated seed is re-avalanched at each step, so
/// combine(combine(s,a),b) != combine(combine(s,b),a). Costs one multiply
/// plus one mix64 per element.
constexpr void combine(std::size_t& seed, std::uint64_t value) noexcept {
  seed = static_cast<std::size_t>(mix64(seed ^ (value * golden64)));
}

/// A type that hashes itself. The primary way to opt a libHSC type in.
template <typename T>
concept self_hashing = requires(const T& t) {
  { t.hash() } -> std::convertible_to<std::size_t>;
};

/// \brief The hashing customization point.
///
/// Resolution order: a member `hash()` (how libHSC types opt in), then the
/// mixers for integral and enumeration types, then `std::hash` for everything
/// else (std types, pointers, strings). One function, no specialization
/// forest: a new libHSC type declares `hash()` and is done.
template <typename T>
[[nodiscard]] std::size_t hash_value(const T& x) noexcept {
  if constexpr (self_hashing<T>) {
    return x.hash();
  } else if constexpr (std::is_enum_v<T>) {
    return static_cast<std::size_t>(
        mix64(static_cast<std::uint64_t>(std::to_underlying(x))));
  } else if constexpr (std::is_integral_v<T>) {
    // signed values convert modularly, which is exactly what a mixer wants
    return static_cast<std::size_t>(mix64(static_cast<std::uint64_t>(x)));
  } else if constexpr (std::is_pointer_v<T>) {
    return static_cast<std::size_t>(
        mix_ptr(reinterpret_cast<std::uintptr_t>(x)));
  } else {
    return std::hash<T>{}(x);
  }
}

/// Fold the hash of \p x into \p seed.
template <typename T>
void hash_combine(std::size_t& seed, const T& x) noexcept {
  combine(seed, hash_value(x));
}

/// Fold a sequence into \p seed, order-sensitively.
template <std::input_iterator It>
void hash_range(std::size_t& seed, It first, It last) noexcept {
  for (; first != last; ++first) hash_combine(seed, *first);
}

/// \brief Hash a composite in one expression: `hash_all(kind_, var_, sons_)`.
///
/// The seed is the argument count, so composites of different arity do not
/// collide through a common prefix.
template <typename... Ts>
[[nodiscard]] std::size_t hash_all(const Ts&... xs) noexcept {
  std::size_t seed = sizeof...(Ts);
  (hash_combine(seed, xs), ...);
  return seed;
}

}  // namespace hsc::util
