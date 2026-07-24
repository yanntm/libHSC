/// \file to_surface.hh
/// \brief M2M: a `dve::model` → surface `datum` forms, and their `.hsc`
/// serialization (M2T).
///
/// Resolution and numbering happen here, after `algorithm.md` §2: `glob_` /
/// `P_` naming, `P_state` control leaves by declaration index, constants
/// folded, effect lists parallelized by forward substitution, channel
/// rendezvous fused per (send, recv) pair. The output is the same tree
/// `.hsc` files parse to; whether a form is separable, crossing, or refused is the
/// surface's judgement, not this layer's.
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "hsc/dve/ast.hh"
#include "hsc/surface/sexpr.hh"

namespace hsc::dve {

/// \brief A transformed model: the surface forms, and notes worth a header
/// comment (transient states seen, dropped unmatched syncs, …).
struct surface_model {
  std::vector<surface::datum> forms;
  std::vector<std::string> notes;
};

/// \brief A model that cannot be mapped (lockstep `system sync`, an effect
/// list no forward substitution parallelizes soundly, …), with its line.
class transform_error : public std::runtime_error {
 public:
  transform_error(int line, const std::string& what)
      : std::runtime_error("line " + std::to_string(line) + ": " + what),
        line_(line) {}
  [[nodiscard]] int line() const noexcept { return line_; }

 private:
  int line_;
};

/// \brief Transform \p m. Throws `transform_error`.
///
/// \p force_order lays the leaves out with the FORCE heuristic
/// (`hsc/order/force.hh`): one clique per event over the leaves it
/// touches, one precedence per (read, addressed cell / written target)
/// pair. `false` keeps DVE declaration order. Order is representation
/// only — state counts are invariant under it.
[[nodiscard]] surface_model to_surface(const model& m,
                                       bool force_order = true);

/// \brief M2T: serialize as `.hsc` text, notes as leading comments.
void print_hsc(std::ostream& os, const surface_model& sm);

}  // namespace hsc::dve
