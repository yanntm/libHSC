#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "hsc/mem/handle.hh"
#include "hsc/mem/intern.hh"

using namespace hsc;

namespace {

/// A fixed-size interned value.
struct point {
  int x, y;
  friend bool operator==(const point&, const point&) = default;
  [[nodiscard]] std::size_t hash() const { return util::hash_all(x, y); }
};

/// \brief A value with a trailing array: the shape a diagram node has.
///
/// The sons follow the header in the same allocation, so a node is one block
/// and one indirection. Nothing about this type is special to the test; it is
/// the case `intern` exists to serve well.
struct arc_node {
  std::uint32_t var;
  std::uint32_t arity;

  [[nodiscard]] const std::uint32_t* sons() const {
    return reinterpret_cast<const std::uint32_t*>(this + 1);
  }
  [[nodiscard]] std::span<const std::uint32_t> arcs() const {
    return {sons(), arity};
  }
  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_value(var);
    util::hash_range(seed, sons(), sons() + arity);
    return seed;
  }
  friend bool operator==(const arc_node& a, const arc_node& b) {
    return a.var == b.var && std::ranges::equal(a.arcs(), b.arcs());
  }
};

/// The probe view for arc_node: describes a node that does not exist yet.
struct arc_view {
  std::uint32_t var;
  std::span<const std::uint32_t> sons;

  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_value(var);
    util::hash_range(seed, sons.begin(), sons.end());
    return seed;
  }
  [[nodiscard]] bool equals(const arc_node& n) const {
    return n.var == var && std::ranges::equal(n.arcs(), sons);
  }
  [[nodiscard]] std::size_t extra_bytes() const {
    return sons.size() * sizeof(std::uint32_t);
  }
  arc_node* construct(void* mem) const {
    auto* p = new (mem) arc_node{var, static_cast<std::uint32_t>(sons.size())};
    std::copy(sons.begin(), sons.end(),
              const_cast<std::uint32_t*>(p->sons()));
    return p;
  }
};

static_assert(mem::interning_view<arc_view, arc_node>);

}  // namespace

TEST_CASE("interning is canonical") {
  mem::intern<point> table;
  const auto a = table.get(point{1, 2});
  const auto b = table.get(point{1, 2});
  const auto c = table.get(point{2, 1});
  CHECK(a == b);
  CHECK(a != c);
  CHECK(a != 0);  // 0 is absence and is never issued
  CHECK(table.size() == 2);
  CHECK(table[a] == point{1, 2});
}

TEST_CASE("a hit allocates nothing and is counted as a hit") {
  mem::intern<point> table;
  for (int i = 0; i < 100; ++i) table.get(point{i, 0});
  for (int i = 0; i < 100; ++i) table.get(point{i, 0});
  const auto& s = table.stats();
  CHECK(s.lookups == 200);
  CHECK(s.misses == 100);
  CHECK(s.hits == 100);
  CHECK(s.size == 100);
  CHECK(s.probes < s.lookups * 3);  // linear probing is not degenerating
}

TEST_CASE("growth preserves identity") {
  mem::intern<point> table(8);  // deliberately tiny: forces rehashes
  std::vector<std::uint32_t> ids;
  for (int i = 0; i < 5000; ++i) ids.push_back(table.get(point{i, i * 2}));
  CHECK(table.stats().rehashes > 0);
  for (int i = 0; i < 5000; ++i) {
    CHECK(table.get(point{i, i * 2}) == ids[static_cast<std::size_t>(i)]);
  }
  CHECK(table.size() == 5000);
}

TEST_CASE("values with trailing arrays intern by view") {
  mem::intern<arc_node> table;
  const std::vector<std::uint32_t> sons{7, 8, 9};
  const auto a = table.get(arc_view{1, sons});
  const auto b = table.get(arc_view{1, sons});
  CHECK(a == b);

  const std::vector<std::uint32_t> other{9, 8, 7};  // permuted: a real node
  CHECK(table.get(arc_view{1, other}) != a);
  CHECK(table.get(arc_view{2, sons}) != a);

  CHECK(table[a].arity == 3);
  CHECK(std::ranges::equal(table[a].arcs(), sons));

  // the lookups above are hits or fresh misses, never speculative builds
  CHECK(table.stats().misses == 3);
}

TEST_CASE("collection keeps what is referenced and frees the rest") {
  mem::intern<point> table;
  const auto kept = table.get(point{1, 1});
  const auto dropped = table.get(point{2, 2});
  table.ref(kept);
  CHECK(table.ref_count(kept) == 1);

  table.collect();

  CHECK(table.size() == 1);
  CHECK(table.stats().collected == 1);
  CHECK(table.resolve(kept) != nullptr);
  CHECK(table.resolve(dropped) == nullptr);
  // the survivor is still canonical after the probe table was rebuilt
  CHECK(table.get(point{1, 1}) == kept);
}

TEST_CASE("collection marks children iteratively") {
  mem::intern<arc_node> table;
  const std::vector<std::uint32_t> none{};
  const auto leaf = table.get(arc_view{0, none});

  // a spine 2000 deep: recursion here is how the legacy marker dies
  std::uint32_t current = leaf;
  for (std::uint32_t i = 1; i <= 2000; ++i) {
    const std::uint32_t son[] = {current};
    current = table.get(arc_view{i, son});
  }
  const auto root = current;
  table.ref(root);
  table.get(arc_view{9999, none});  // garbage, unreachable from root

  table.collect([](const arc_node& n, auto&& push) {
    for (auto son : n.arcs()) push(son);
  });

  CHECK(table.size() == 2001);  // the spine and its leaf
  CHECK(table.stats().collected == 1);
  CHECK(table.resolve(root) != nullptr);
  CHECK(table.resolve(leaf) != nullptr);
}

TEST_CASE("freed ids are recycled and their generation bumped") {
  mem::intern<point> table;
  const auto doomed = table.get(point{1, 1});
  const auto gen = table.generation(doomed);
  const mem::certificate<point> cert(table, mem::weak<point>(doomed));
  CHECK(cert.valid(table));

  table.collect();
  CHECK_FALSE(cert.valid(table));  // the citation is knowably stale

  const auto reborn = table.get(point{5, 5});
  CHECK(reborn == doomed);  // the id came back off the free list
  CHECK(table.stats().recycled == 1);
  CHECK(table.generation(reborn) != gen);
  CHECK_FALSE(cert.valid(table));  // and still stale, not confused with this
  CHECK(cert.get(table) == mem::weak<point>());
}

TEST_CASE("strong handles are roots, weak handles are not") {
  mem::intern<point> table;
  const auto id = table.get(point{3, 4});
  {
    const mem::strong<point> root(table, mem::weak<point>(id));
    CHECK(table.ref_count(id) == 1);
    {
      const mem::strong<point> copy = root;  // NOLINT: the point of the test
      CHECK(table.ref_count(id) == 2);
    }
    CHECK(table.ref_count(id) == 1);
    CHECK(*root == point{3, 4});

    table.collect();
    CHECK(table.size() == 1);
  }
  CHECK(table.ref_count(id) == 0);
  table.collect();
  CHECK(table.size() == 0);
}

TEST_CASE("weak handles are trivially copyable and hashable") {
  static_assert(std::is_trivially_copyable_v<mem::weak<point>>);
  static_assert(sizeof(mem::weak<point>) == sizeof(std::uint32_t));
  mem::intern<point> table;
  const mem::weak<point> w(table.get(point{1, 2}));
  CHECK(w);
  CHECK_FALSE(mem::weak<point>{});
  CHECK(w(table) == point{1, 2});
  CHECK(w.hash() == util::hash_value(w));
}
