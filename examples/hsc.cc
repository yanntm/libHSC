/// \file hsc.cc
/// \brief Run a model from a `.hsc` file: parse, expand, translate, execute,
/// report.
///
/// The whole surface behind one entry point. Exit code is 0 on success,
/// nonzero on a parse/expand/translate error or a failed `expect` — so a
/// model file is a self-checking test. `--expand` stops after the parametric
/// pass and prints the expanded model as plain `.hsc` text: the
/// degeneralization of a parametric file, itself runnable.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "hsc/surface/expand.hh"
#include "hsc/surface/translate.hh"

namespace {

int dump_expanded(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    std::cerr << "cannot open " << path << '\n';
    return 2;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  try {
    // The dump enumerates families too: its output is the flat, diffable,
    // runnable degeneralization.
    for (const hsc::surface::datum& form : hsc::surface::expand(
             hsc::surface::parse(buf.str()), /*families=*/false)) {
      hsc::surface::write(std::cout, form);
      std::cout << '\n';
    }
    return 0;
  } catch (const hsc::surface::parse_error& e) {
    std::cerr << path << ": parse error: " << e.what() << '\n';
  } catch (const hsc::surface::expand_error& e) {
    std::cerr << path << ": expand error: " << e.what() << '\n';
  }
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--expand")
    return dump_expanded(argv[2]);
  if (argc != 2) {
    std::cerr << "usage: " << (argc ? argv[0] : "hsc")
              << " [--expand] <model.hsc>\n";
    return 2;
  }
  return hsc::surface::run_file(argv[1], std::cout, std::cerr);
}
