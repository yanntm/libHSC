/// \file state.hh
/// \brief Concrete states: one int32 per frontier position, in shape order.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace hsc::xpl {

/// A leaf value. The same width as everywhere in libHSC: leaving int32 is
/// a loud error, never a wrap.
using value = std::int32_t;

/// Dense id of an interned state, allocation order.
using state_id = std::uint32_t;

/// A read-only view of one state: `arity` values, frontier order — exactly
/// the `env` that `lia` evaluation reads.
using state_view = std::span<const value>;

/// An owned state, for building successors.
using word = std::vector<value>;

}  // namespace hsc::xpl
