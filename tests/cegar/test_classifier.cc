// M1 — classifiers: canonicalization idempotence, language equality ⇒
// byte equality, chaos, classification.

#include <doctest/doctest.h>

#include "hsc/cegar/classifier.hh"

using namespace hsc::cegar;

TEST_CASE("chaos accepts everything and is canonical") {
  classifier h = chaos(3);
  CHECK(h.k == 1);
  CHECK(h.dead == -1);
  CHECK(h.accepts({}));
  CHECK(h.accepts({0, 1, 2, 2, 1, 0}));
  raw_table t{3, 1, {0, 0, 0}, {true}};
  CHECK(canonicalize(t).serialize() == h.serialize());
}

TEST_CASE("canonicalize merges dead classes and minimizes") {
  // Language a* over {a, b}: live 0 loops on a; b goes to either of two
  // dead classes that wander between themselves — all must collapse.
  raw_table t;
  t.n_letters = 2;
  t.k = 3;
  t.act = {0, 1,   // live: a->self, b->dead1
           2, 1,   // dead1 -> dead2 / dead1
           1, 0};  // dead2 wanders (even back to live: closure cuts it)
  t.live = {true, false, false};
  classifier h = canonicalize(t);
  CHECK(h.k == 2);
  CHECK(h.dead == 1);
  CHECK(h.accepts({0, 0, 0}));
  CHECK(!h.accepts({0, 1, 0}));  // dead is absorbing: b condemns the cone
  // Idempotence: canonicalize of the canonical table is byte-identical.
  raw_table t2{h.n_letters, h.k, h.act, {true, false}};
  CHECK(canonicalize(t2).serialize() == h.serialize());
}

TEST_CASE("language equality gives byte equality across presentations") {
  // Words over {a,b} with no b: presented with 4 classes, permuted and
  // padded with redundant live classes.
  raw_table t1;
  t1.n_letters = 2;
  t1.k = 4;
  t1.act = {2, 1,   // 0: a->2 (equiv 0), b->1 (dead)
            1, 1,   // 1: dead
            0, 3,   // 2: a->0, b->3 (dead too)
            3, 3};  // 3: dead
  t1.live = {true, false, true, false};
  raw_table t2;
  t2.n_letters = 2;
  t2.k = 2;
  t2.act = {0, 1, 1, 1};
  t2.live = {true, false};
  CHECK(canonicalize(t1).serialize() == canonicalize(t2).serialize());
}

TEST_CASE("shortlex naming: reps are in shortlex order") {
  // a then b required: classes ε, a, ab, dead.
  raw_table t;
  t.n_letters = 2;
  t.k = 4;
  t.act = {1, 3,   // ε: a->[a], b->dead
           3, 2,   // a: a->dead, b->[ab]
           3, 3,   // ab: absorbs to dead
           3, 3};
  t.live = {true, true, true, false};
  classifier h = canonicalize(t);
  REQUIRE(h.k == 4);
  CHECK(h.reps[0] == word{});
  CHECK(h.reps[1] == word{0});
  CHECK(h.reps[2] == word{1});  // shortlex: 'b' (dead) precedes 'ab'
  CHECK(h.reps[3] == word{0, 1});
  CHECK(h.dead == 2);
}
