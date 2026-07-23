/// \file hsc-mcc.cc
/// \brief Answer an MCC examination on a PNML/NUPN net.
///
///     hsc-mcc <model.pnml> -mcc <StateSpace|OneSafe>
///
/// Import (expat) → emit the model plus the examination's query in the `.hsc`
/// surface → parse and translate in-process. The MCC protocol is this
/// runner's: the surface answers general queries (a cardinal, a per-leaf
/// maximum) and the examination verdict is formatted here.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include "hsc/util/errors.hh"

#include "hsc/petri/PTNetLoader.h"
#include "hsc/petri/decompose.hh"
#include "hsc/petri/nupn.hh"
#include "hsc/petri/to_surface.hh"
#include "hsc/surface/sexpr.hh"
#include "hsc/surface/translate.hh"

int main(int argc, char** argv) {
  std::string in, exam;
  bool decompose = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "-mcc" && i + 1 < argc) exam = argv[++i];
    else if (a == "--decompose") decompose = true;
    else if (in.empty()) in = a;
  }
  if (in.empty() || exam.empty()) {
    std::fprintf(stderr,
                 "usage: %s <model.pnml> -mcc <StateSpace|OneSafe>\n",
                 argv[0]);
    return 2;
  }

  hsc::petri::emit_options opts;
  if (exam == "StateSpace") opts.exam = hsc::petri::examination::state_space;
  else if (exam == "OneSafe") opts.exam = hsc::petri::examination::one_safe;
  else {
    std::fprintf(stderr, "unknown examination '%s'\n", exam.c_str());
    return 2;
  }

  SparsePetriNet<int>* net = loadXML<int>(in);
  if (!net) { std::fprintf(stderr, "failed to parse %s\n", in.c_str()); return 1; }
  const hsc::petri::unit_tree units =
      decompose ? hsc::petri::decompose(*net) : hsc::petri::read_units(in);

  std::ostringstream model;
  hsc::petri::to_surface(model, *net, units, opts);
  delete net;

  const std::vector<hsc::surface::datum> forms =
      hsc::surface::parse(model.str());

  if (opts.exam != hsc::petri::examination::one_safe) {
    return hsc::surface::translate(forms, std::cout) == 0 ? 0 : 1;
  }

  // OneSafe: the surface reports the general statistic (`R max-value V`);
  // the MCC protocol — verdict line, unbounded means some place exceeds 1 —
  // is this runner's job, not the calculus's.
  constexpr const char* TECHNIQUES =
      " TECHNIQUES DECISION_DIAGRAMS SATURATION\n";
  std::ostringstream got;
  try {
    if (hsc::surface::translate(forms, got) != 0) return 1;
  } catch (const hsc::overflow_error&) {
    std::cout << "FORMULA OneSafe FALSE" << TECHNIQUES;
    return 0;
  }
  const std::string text = got.str();
  const auto at = text.find("max-value ");
  if (at == std::string::npos) {
    std::fprintf(stderr, "no max-value in surface output\n");
    return 1;
  }
  const long mx = std::strtol(text.c_str() + at + 10, nullptr, 10);
  std::cout << "FORMULA OneSafe " << (mx <= 1 ? "TRUE" : "FALSE")
            << TECHNIQUES;
  return 0;
}
