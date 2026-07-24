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
#include <map>
#include <sstream>
#include <string>

#include "hsc/surface/expand.hh"
#include "hsc/surface/translate.hh"

namespace {

int dump_expanded(const std::string& path,
                  const std::map<std::string, long long>& params) {
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
             hsc::surface::parse(buf.str()), /*families=*/false, params)) {
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
  bool dump = false;
  std::map<std::string, long long> params;  // -DNAME=VALUE param overrides
  std::string path;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--expand") {
      dump = true;
    } else if (a.rfind("-D", 0) == 0) {
      const auto eq = a.find('=');
      if (eq == std::string::npos || eq == 2) {
        std::cerr << a << ": expected -DNAME=VALUE\n";
        return 2;
      }
      try {
        params[a.substr(2, eq - 2)] = std::stoll(a.substr(eq + 1));
      } catch (const std::exception&) {
        std::cerr << a << ": expected an integer value\n";
        return 2;
      }
    } else if (path.empty()) {
      path = a;
    } else {
      path.clear();
      break;
    }
  }
  if (path.empty()) {
    std::cerr << "usage: " << (argc ? argv[0] : "hsc")
              << " [--expand] [-DNAME=VALUE…] <model.hsc>\n";
    return 2;
  }
  if (dump) return dump_expanded(path, params);
  return hsc::surface::run_file(path, std::cout, std::cerr, params);
}
