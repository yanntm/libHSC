/// \file model.hh
/// \brief The explicit transition relation: events as terms of the event
/// algebra (filter/update/seq/alt/abort) over `lia` codes and frontier
/// positions, plus the supports the engine's maintenance walks.
///
/// Semantics in `algorithm.md` beside this file. The model is immutable
/// during a run; `finalize()` computes the supports once, after which
/// everything is shared read-only.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hsc/lia/expr.hh"
#include "hsc/petri/SparseBoolArray.h"
#include "hsc/xpl/state.hh"

namespace hsc::xpl {

/// One write target: a scalar frontier position, or an array access whose
/// index is evaluated at fire time.
struct target {
  std::vector<std::uint32_t> cells;  ///< scalar: one entry, no index
  lia::iexpr index = 0;
  bool indexed = false;
};

/// One action of an update: an assignment or a havoc over a range.
struct action {
  enum class kind : std::uint8_t { assign, havoc };
  kind k = kind::assign;
  target lhs;
  lia::iexpr rhs = 0;  ///< assign: the value, read on the update pre-state
  value lo = 0;        ///< havoc: any value of [lo, hi)
  value hi = 0;
};

/// An update is a simultaneous multi-assign.
using clause = std::vector<action>;

/// One term of the event algebra. Terms live in the model's pool; kids
/// are pool indices. No lfp: closures stay outside events for now.
struct term {
  enum class kind : std::uint8_t { filter, update, seq, alt, abort };
  kind k = kind::filter;
  lia::bexpr guard = lia::btrue;    ///< filter
  clause acts;                      ///< update
  std::vector<std::uint32_t> kids;  ///< seq / alt
};

struct event {
  std::string name;
  int line = 0;
  std::uint32_t root = 0;  ///< pool index of the event's term

  /// A *necessary* enabling condition: exact for the guard/deterministic
  /// shape (the hoisted pre-state guard), `btrue` when nothing cheaper is
  /// known. The engine's may-fire sets are over this; `fire` is
  /// authoritative.
  lia::bexpr quick = lia::btrue;

  // Supports, filled by model::finalize() (sorted position sets):
  SparseBoolArray ctrl;    ///< support of `quick` — what may-fire watches
  SparseBoolArray reads;   ///< every read in the term: filters, rhs, indexes
  SparseBoolArray writes;  ///< targets, conservative: dynamic array = all cells
};

struct model {
  std::size_t arity = 0;
  const lia::expr_factory* ex = nullptr;
  std::vector<term> pool;
  std::vector<event> events;

  /// Inverse of ctrl: `readers[p]` = events whose `quick` reads position p.
  std::vector<SparseBoolArray> readers;

  std::uint32_t add(term t) {
    pool.push_back(std::move(t));
    return static_cast<std::uint32_t>(pool.size() - 1);
  }

  /// Compute every event's supports and the readers index. Call once,
  /// after the events are in place.
  void finalize();
};

}  // namespace hsc::xpl
