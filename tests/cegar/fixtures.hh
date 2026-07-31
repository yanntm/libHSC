// Test support: `.hsc` literals through the real pipeline — parse,
// expand, spec read, cegar bridge — exactly the runner's path.
#pragma once

#include <string>

#include "hsc/surface/cegar_build.hh"
#include "hsc/surface/expand.hh"

namespace hsc::cegar::testing {

/// Build a cegar model from `.hsc` text and a property atom list
/// ("(== mon 2)"). Throws like the runner does on fragment refusals.
inline surface::cegar_bridge build(const std::string& text,
                                   const std::string& atoms) {
  using namespace hsc::surface;
  const std::vector<datum> forms = expand(parse(text), /*families=*/true, {});
  const spec s = spec::read(forms);
  lia::expr_factory ex;
  const expr_reader reader(ex, s);
  const std::vector<datum> adat = parse(atoms);
  return build_cegar_model(s, ex, reader, adat);
}

/// The paper's §6 worked example, two clients: server + interned
/// clients + a grant-counting monitor leaf. Property: (== mon 2).
inline const char* kClients2 = R"(
(leaf srv 0 3)  ; 0 free, i busy_i
(leaf cl1 0 2)  ; 0 idle, 1 busy
(leaf cl2 0 2)
(leaf mon 0 3)  ; grants outstanding; 2 = a grant on top of a grant
(shape (spine srv cl1 cl2 mon))
(init)
(event g1 (when (== srv 0) (== cl1 0) (< mon 2))
          (do (:= srv 1) (:= cl1 1) (+= mon 1)))
(event r1 (when (== srv 1) (== cl1 1) (== mon 1))
          (do (:= srv 0) (:= cl1 0) (-= mon 1)))
(event g2 (when (== srv 0) (== cl2 0) (< mon 2))
          (do (:= srv 2) (:= cl2 1) (+= mon 1)))
(event r2 (when (== srv 2) (== cl2 1) (== mon 1))
          (do (:= srv 0) (:= cl2 0) (-= mon 1)))
)";

/// The bugged variant: grants no longer check the server is free.
inline const char* kClients2Bug = R"(
(leaf srv 0 3)
(leaf cl1 0 2)
(leaf cl2 0 2)
(leaf mon 0 3)
(shape (spine srv cl1 cl2 mon))
(init)
(event g1 (when (== cl1 0) (< mon 2)) (do (:= srv 1) (:= cl1 1) (+= mon 1)))
(event r1 (when (== srv 1) (== cl1 1) (== mon 1))
          (do (:= srv 0) (:= cl1 0) (-= mon 1)))
(event g2 (when (== cl2 0) (< mon 2)) (do (:= srv 2) (:= cl2 1) (+= mon 1)))
(event r2 (when (== srv 2) (== cl2 1) (== mon 1))
          (do (:= srv 0) (:= cl2 0) (-= mon 1)))
)";

}  // namespace hsc::cegar::testing
