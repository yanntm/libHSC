/// \file props_to_surface.hh
/// \brief M2T for properties: the PetriSpot property tree as surface query
/// atoms over the leaves of the emitted model.
///
/// Text only, like `to_surface.hh`: nothing here links the calculus. The
/// leaves of the emitted model are the net's place names, so a place index
/// prints as `pnames[i]`. The surface's atom language (manual §8): `and`,
/// `or` n-ary, `not`, comparisons `== != <= >= < >` between a linear form
/// and a constant, a linear form as a right-nested binary `+` of `(* c p)`
/// terms (`p` alone for a unit coefficient). The surface has no constant
/// atom: a constant is encoded as a tautology or a contradiction on the
/// first leaf.
#pragma once

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "hsc/petri/core/SparsePetriNet.h"
#include "hsc/petri/expr/CtlFormula.h"
#include "hsc/petri/expr/Expression.h"

namespace hsc::petri {

/// The linear form of \p a (its terms, not the comparison) as a surface
/// expression; `0` when it has no term.
std::string linear_form(const ::petri::expr::LinearAtom& a,
                        const std::vector<std::string>& pnames);

/// \p a as a comparison atom `(CMP form constant)`.
std::string atom_text(const ::petri::expr::LinearAtom& a,
                      const std::vector<std::string>& pnames);

/// \p e as a boolean query atom. Constants become `(>= p0 0)` / `(<= p0 -1)`
/// with `p0 = pnames[0]`; the caller folds root constants before asking.
std::string query_atom(const ::petri::expr::Expression& e,
                       const std::vector<std::string>& pnames);

/// The enabling condition of transition \p t: `(and (>= p w) ...)` over its
/// pre-arcs, a single comparison when there is one; nullopt when the preset
/// is empty (the transition is always enabled).
std::optional<std::string> guard_atom(const SparsePetriNet<int>& net,
                                      std::size_t t);

/// The dead markings: `(not (or G_1 ... G_n))` over every transition's
/// guard; nullopt when some transition is always enabled (no dead marking).
std::optional<std::string> deadlock_atom(const SparsePetriNet<int>& net);

/// \p f as a surface CTL formula (manual §8f): predicates as query atoms,
/// constants as `true` / `false`, the deadlock atom as `(deadlock)`, the
/// path operators by name.
std::string ctl_text(const ::petri::expr::CtlFormula& f,
                     const std::vector<std::string>& pnames);

/// The comparison `form >= k` for a linear form, as an atom.
std::string at_least(const ::petri::expr::LinearAtom& form, long long k,
                     const std::vector<std::string>& pnames);

}  // namespace hsc::petri
