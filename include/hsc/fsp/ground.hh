/// \file ground.hh
/// \brief M2LTS: one FSP process → its ground LTS (`algorithm.md` §2).
///
/// States are enumerated from the initial reference in BFS discovery order
/// (initial = 0), label patterns expanded to ground labels, guards and
/// target arithmetic evaluated under constants + parameters + binders,
/// hiding applied. `ERROR` is not numbered: a transition into it carries
/// destination `-1` (the property's totalization gives it a value later).
#pragma once

#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "hsc/fsp/ast.hh"

namespace hsc::fsp {

/// \brief A ground label: the atom sequence (names and decimal integers);
/// `[i]` indexing and `.` both concatenate.
using glabel = std::vector<std::string>;

/// \brief One process, grounded.
struct ground_lts {
  std::string name;
  bool is_property = false;
  /// Printable state names, index = state number (0 = initial).
  std::vector<std::string> state_names;
  /// Per label, the transition pairs (src, dst); dst == -1 is ERROR.
  std::map<glabel, std::vector<std::pair<int, int>>> rel;
  /// The alphabet: labels of `rel` union the grounded `+{…}` extension.
  std::set<glabel> alphabet;
  /// Hidden (tau) edges, process-local after `\{…}`.
  std::vector<std::pair<int, int>> tau;
};

/// \brief A model that does not ground (unknown name, argument outside its
/// declared range, division by zero, …), with the offending line.
class ground_error : public std::runtime_error {
 public:
  ground_error(int line, const std::string& what)
      : std::runtime_error("line " + std::to_string(line) + ": " + what),
        line_(line) {}
  [[nodiscard]] int line() const noexcept { return line_; }

 private:
  int line_;
};

/// \brief Ground process \p p of model \p m. Throws `ground_error`.
[[nodiscard]] ground_lts ground(const model& m, const process& p);

/// \brief Render a ground label for humans: atoms dot-joined.
[[nodiscard]] std::string to_string(const glabel& l);

}  // namespace hsc::fsp
