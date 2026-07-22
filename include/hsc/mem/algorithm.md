# `mem/` — algorithms

Abstract description of what the code in this folder does. Written before the
code; kept in step with it.

## 1. The unique table (`intern.hh`)

One mechanism, instantiated for every kind of value libHSC needs to be
canonical: diagram nodes, shapes, operation terms, residuals, kernels. A
value that has been interned is addressed by an **id**, a 32-bit integer by
default. Ids are shorter than pointers because they sit on edges, and an edge
is the thing a decision diagram has billions of; at 4G distinct values of one
kind we are dead in the water anyway. `Id` is a parameter all the same — the
design is not hostile to wider ids.

### Structures

| name | shape | purpose | bytes/entry |
|---|---|---|---|
| `index_` | `vector<T*>` | id → value. The array direction, O(1). | 8 |
| `table_` | `id_table` | value → id. The content-addressed direction. | ~5.7 |
| `generations_` | `vector<uint32>` | id reuse counter, for certificates | 4 |
| `marks_` | `vector<bool>` | mark & sweep bitset | 0.125 |
| `refs_` | `google::sparsetable<Id>` | refcounts of *referenced* ids only | ~0.3 |
| `free_` | `vector<Id>` | free list, explicit | 4 per free slot |

Id `0` is never allocated: it is "absence" everywhere in libHSC, and the
probe table's empty marker.

`refs_` is sparse because it is overwhelmingly empty — refcounts exist only
for values held by a root handle, which is a vanishing fraction of the live
set. This is the one place sparse storage genuinely pays, and it is why
google sparsehash is a dependency. The free list is an explicit vector rather
than legacy's chain threaded through `index_` by reinterpreting ids as
pointers (`UniqueTableId.hh:112-135`): the punning bought 0 bytes per *live*
node and cost type-safety.

### Interning: probe by view, allocate only on miss

The central operation is "give me the id of the value with *this* content",
asked before that content exists as an object. The content is presented as a
**view**: a light struct, usually holding references and spans into caller
memory, satisfying `interning_view<V, T>`:

    v.hash()          -> size_t          hash of the value it describes
    v.equals(stored)  -> bool            compare against an interned T
    v.extra_bytes()   -> size_t          trailing bytes beyond sizeof(T)
    v.construct(mem)  -> T*              placement-new into mem

Then:

    get(view):
      slot = probe table_ with view.hash(), comparing view.equals(index_[id])
      if a slot holds an id: hit, return it                 # no allocation
      miss:
        mem = new char[sizeof(T) + view.extra_bytes()]
        p   = view.construct(mem)
        id  = free_.empty() ? grow() : pop(free_)
        index_[id] = p ; table_.place(slot, id)
        return id

Two properties, both deliberate:

* **Nothing is allocated on a hit**, and hits dominate. This is the whole
  reason the probe set is ours (§2) rather than `sparse_hash_set`, whose
  `find` accepts only a key of the stored type and therefore forces either a
  sentinel-id slot (legacy `UniqueTableId.hh:265`) or a throwaway object.
* **Nothing is constructed speculatively.** libsdd constructs first and
  discovers the hit afterwards, which is why it needs an allocation cache
  (`unique_table::allocate` keeps the largest freed block). Probing by view
  removes the churn that cache exists to absorb, so we do not adopt it; if
  measurement later shows allocator pressure from `collect()`'s freed
  blocks, it comes back, sized by class.

Trailing arrays are supported from the start (`extra_bytes` + placement new),
because the node type that matters at M2/M6 is a header followed by its arcs
and we do not want to re-lay the foundation for it.

### Collection

Mark and sweep, roots being the ids with a non-zero refcount.

    collect(mark_children):
      worklist = ids with refs_ non-zero
      while worklist:                       # explicit worklist, not recursion
        id = pop
        if marks_[id]: continue
        marks_[id] = true
        mark_children(*index_[id], push)    # owner names the cited ids
      for each id in table_:
        if not marks_[id]:
          destroy index_[id]; free_.push(id); ++generations_[id]
      table_ = rebuilt from the surviving ids   # swapped in, not copied

Marking is iterative because diagram spines are deep and the legacy recursive
`mark()` is a stack-overflow waiting to happen. `mark_children` is supplied by
the owner rather than demanded of `T`, because a value in one table cites ids
in *other* tables (a node cites leaf codes); only the owner knows the graph.

The sweep **rebuilds** the probe table from the survivors instead of erasing
entries one by one. Rebuilding is why no tombstone marker is needed, which is
why the probe table can be a bare array of ids.

### Generations, and why they are here at M1

Freeing an id bumps `generations_[id]`. A **certificate** is a pair (id,
generation); `valid(id, gen)` answers whether the id still denotes what the
certificate holder meant. Nothing on the hot path checks generations — edges
carry bare ids and pay nothing — but a cache entry, a retention policy, or
any structure that must survive a collection can hold a certificate and know
whether it went stale. This is what legacy structurally lacks: an entry citing
a dead id is indistinguishable from one citing that id's next life, which is
why collection there must wipe every cache. Cheap now, prerequisite for the
policies of M7.

## 2. The probe set (`id_table.hh`)

A flat `vector<Id>`, power-of-two sized, open addressing with linear probing,
`0` = empty. No tombstones: entries are only ever removed by the wholesale
rebuild that `collect()` performs.

It stores ids and nothing else — no hashes, no pointers — and delegates both
hashing and comparison to callables supplied per call, so it is not
templated on the value type at all. Lookup follows the check/commit shape:
`probe()` reports either a hit or the slot a new id belongs in, and
`place()` commits into that slot. Nothing may grow the table in between,
which is exactly the miss path's discipline.

Growth at load factor 0.7, doubling, by rehashing every stored id.

## 3. Handles (`handle.hh`)

* `weak<T>` — a bare id in a typed wrapper. Trivially copyable, no
  refcount traffic. This is what edges hold and what crosses function
  boundaries in the hot path.
* `strong<T>` — table pointer plus id, refcounting on copy and destruction.
  This is what *roots* are: variables in user code, the operands a cache
  entry holds. Two words, and never on an edge.
* `certificate<T>` — id plus generation, for holders that must outlive a
  collection.

The weak/strong split is libDDD's `GDDD`/`DDD` distinction, kept because it
is right: libsdd refcounts on every copy, which puts atomic-free but real
traffic on the hottest path in the library.

## 4. The operation cache (`cache.hh`)

Adapted from libsdd's `sdd/mem/cache.hh`, `cache_entry.hh`, `lru_list.hh` and
`hash_table.hh` (Alexandre Hamez, BSD-2), with the deltas noted below.

**Operation as key.** There is no key/value pair: the operation object *is*
the key, and it knows how to compute its own result. A client type supplies
`hash()`, `operator==`, and `result_type operator()(Context&)`. The cache is
then `cache<Context, Operation>` — two template parameters, both load-bearing,
and no variadic filter chain.

**Fixed capacity, all memory at construction.** Buckets and entries are
allocated once; the table never rehashes and the pool never grows. A cache
that cannot grow cannot surprise you, and a fixed bucket array means bucket
addresses are stable across a re-entrant evaluation.

**Everything intrusive.** An entry carries its own bucket chain link *and*
its own LRU links. libsdd keeps the LRU order in a `std::list<entry*>`, which
mallocs a list node on every miss; ours costs two pointers in an entry that
was going to be allocated anyway.

    entry: bucket_next | older | newer | hash | operation | result

    lookup(op):
      if not admitted(op): ++filtered ; return op(cxt)      # never stored
      h = op.hash()
      walk bucket h & mask:
        if entry.hash == h and entry.operation == op:
          ++hits ; move entry to the MRU end ; return entry.result
      ++misses
      result = op(cxt)          # may re-enter this cache
      if full: evict()
      insert(op, result)        # re-walks the bucket: see below
      return result

**Evaluate, then re-probe.** libsdd computes the insertion point *before*
evaluating and commits into it afterwards. Evaluating an operation
re-enters the cache (that is the whole point of a memoized recursion), so we
re-walk the bucket at insertion time and yield to an entry a nested call may
have placed there. One extra chain walk, on the miss path only, where an
entire operation has just been evaluated.

**Eviction: LRU, in batches of K.** K is a constructor knob, default 1
(libsdd's behaviour). The roadmap asks for batching on the grounds that
evicting one entry per insertion at capacity thrashes. The reasoning does not
obviously hold — total eviction work is the same either way, and evicting K
discards K−1 entries that were still wanted — so it ships as a knob with a
measurement (`bench/bench_cache.cc`) rather than as a default.

**Admission** is two questions, both cheap: an optional `cacheable()` on the
operation (the operation knows whether it is worth storing — a compile-time
question, answered by a concept, costing nothing when absent), and an
optional runtime filter installed on the cache (the policy hook M7 grows
into). No compile-time filter chain.

**Lifetime, and why there is no GC event to survive.** An operation holds its
operands, and it should hold them as `strong` handles. Then a live entry
keeps its operands alive, retention is continuous, and collection has nothing
to invalidate — the wipe-all-caches-at-GC problem is *dissolved* rather than
solved. Whether an entry is *retained* is then policy (M7), and memory is
bounded by live roots plus cache capacity. The cache itself takes no position:
it stores what the operation type holds. Clients that store `weak` handles
instead must hold `certificate`s and check them, which is what generations
are for.
