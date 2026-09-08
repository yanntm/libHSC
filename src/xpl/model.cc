/// \file model.cc
/// \brief Supports and the readers index: what the may-fire test depends
/// on, and the term's full read/write footprint. `interpret/algorithm.md`
/// §5.

#include <algorithm>
#include <cstdint>

#include "hsc/xpl/interpret/model.hh"

namespace hsc::xpl {

namespace {

void take(std::vector<std::uint32_t>& dst, std::vector<std::uint32_t> src) {
  dst.insert(dst.end(), src.begin(), src.end());
}

/// Sort, deduplicate, and load \p ps into a sorted sparse set.
SparseBoolArray as_set(std::vector<std::uint32_t>& ps) {
  std::sort(ps.begin(), ps.end());
  ps.erase(std::unique(ps.begin(), ps.end()), ps.end());
  SparseBoolArray out;
  for (const std::uint32_t p : ps) out.append(p, true);
  return out;
}

/// Reads of one integer expression: scalar support plus array cells.
void expr_reads(const lia::expr_factory& ex, lia::iexpr e,
                std::vector<std::uint32_t>& reads) {
  take(reads, ex.support(e));
  take(reads, ex.array_positions(e));
}

/// The term's footprint, walking the pool from \p ti.
void term_supports(const model& m, std::uint32_t ti,
                   std::vector<std::uint32_t>& reads,
                   std::vector<std::uint32_t>& writes) {
  const term& t = m.pool[ti];
  switch (t.k) {
    case term::kind::filter:
      take(reads, m.ex->support_bool(t.guard));
      take(reads, m.ex->array_positions_bool(t.guard));
      return;
    case term::kind::update:
      for (const action& a : t.acts) {
        if (a.k == action::kind::assign) expr_reads(*m.ex, a.rhs, reads);
        if (a.lhs.indexed) {
          expr_reads(*m.ex, a.lhs.index, reads);
          // dynamic target: conservatively every cell is writable
          writes.insert(writes.end(), a.lhs.cells.begin(),
                        a.lhs.cells.end());
        } else {
          writes.push_back(a.lhs.cells.front());
        }
      }
      return;
    case term::kind::seq:
    case term::kind::alt:
      for (const std::uint32_t kid : t.kids) {
        term_supports(m, kid, reads, writes);
      }
      return;
    case term::kind::abort:
      return;
  }
}

}  // namespace

void model::finalize() {
  readers.assign(arity, SparseBoolArray());
  for (std::size_t ei = 0; ei < events.size(); ++ei) {
    event& e = events[ei];
    std::vector<std::uint32_t> ctrl = ex->support_bool(e.quick);
    take(ctrl, ex->array_positions_bool(e.quick));
    std::vector<std::uint32_t> reads;
    std::vector<std::uint32_t> writes;
    term_supports(*this, e.root, reads, writes);
    take(reads, std::vector<std::uint32_t>(ctrl));  // quick reads are reads
    e.ctrl = as_set(ctrl);
    e.reads = as_set(reads);
    e.writes = as_set(writes);
    for (const std::uint32_t p : ctrl) readers[p].append(ei, true);
  }
}

}  // namespace hsc::xpl
