/// \file nupn2hsc.cc
/// \brief Import a PNML/NUPN net into the `.hsc` surface.
///
/// T2M: the vendored PetriSpot parser reads the P/T net; a second SAX pass reads
/// the NUPN unit tree. M2T: `to_surface` serialises both as a `.hsc` model. The
/// tool emits text only — it does not link the calculus core.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "hsc/petri/PTNetLoader.h"
#include "hsc/petri/nupn.hh"
#include "hsc/petri/to_surface.hh"

int main(int argc, char** argv) {
  std::string in, out;
  hsc::petri::emit_options opts;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "-o" && i + 1 < argc) out = argv[++i];
    else if (a == "--bound" && i + 1 < argc) opts.bound = std::atoi(argv[++i]);
    else if (in.empty()) in = a;
    else { std::fprintf(stderr, "unexpected argument '%s'\n", a.c_str()); return 2; }
  }
  if (in.empty()) {
    std::fprintf(stderr, "usage: %s <in.pnml> [-o out.hsc] [--bound N]\n", argv[0]);
    return 2;
  }

  SparsePetriNet<int>* net = loadXML<int>(in);
  if (!net) { std::fprintf(stderr, "failed to parse %s\n", in.c_str()); return 1; }
  const hsc::petri::unit_tree units = hsc::petri::read_units(in);

  if (out.empty()) {
    hsc::petri::to_surface(std::cout, *net, units, opts);
  } else {
    std::ofstream os(out);
    if (!os) { std::fprintf(stderr, "cannot write %s\n", out.c_str()); delete net; return 2; }
    hsc::petri::to_surface(os, *net, units, opts);
  }
  delete net;
  return 0;
}
