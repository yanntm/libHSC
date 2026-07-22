/// \file hsc.cc
/// \brief Run a model from a `.hsc` file: parse, translate, execute, report.
///
/// The whole surface behind one entry point. Exit code is 0 on success,
/// nonzero on a parse/translate error or a failed `expect` — so a model file
/// is a self-checking test.

#include <iostream>

#include "hsc/surface/translate.hh"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << (argc ? argv[0] : "hsc") << " <model.hsc>\n";
    return 2;
  }
  return hsc::surface::run_file(argv[1], std::cout, std::cerr);
}
