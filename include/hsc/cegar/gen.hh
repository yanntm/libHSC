/// \file cegar/gen.hh
/// \brief Model families for tests and sweeps, deterministic in a seed.
///
/// Every generated model records its family and parameters; the emitted
/// `.cts` header comment is the reproducibility record.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "hsc/cegar/model.hh"

namespace hsc::cegar {

/// Paper §6 generalized: one server, k interned-identical clients,
/// "no grant while a grant is outstanding". Holds.
[[nodiscard]] model gen_clients(std::int32_t k);

/// Same, with a server that fails to block clients 1 and 2 overlapping.
/// Violates, shortest witness length 2.
[[nodiscard]] model gen_clients_bug(std::int32_t k);

/// Token ring of n interned-identical stations, mutual-exclusion
/// monitor. Holds.
[[nodiscard]] model gen_ring(std::int32_t n);

/// Random model: l leaves of up to q states over up to s letters
/// (partial deterministic), e events with support density d in [0,1],
/// random monitor with a random bad set. Both verdicts occur across
/// seeds; instances are not trimmed here — trim on `mono` cost at the
/// harness. Deterministic in seed.
[[nodiscard]] model gen_rand(std::int32_t l, std::int32_t q,
                             std::int32_t s, std::int32_t e, double d,
                             std::uint64_t seed);

}  // namespace hsc::cegar
