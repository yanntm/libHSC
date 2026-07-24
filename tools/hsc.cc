/// \file hsc.cc
/// \brief The `hsc` CLI: run a model from a `.hsc` file — parse, expand,
/// translate, execute, report.
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
#include <vector>

#include <CLI/CLI.hpp>

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
  CLI::App app{
      "hsc — run a .hsc model: parse, expand, translate, execute.\n"
      "Exit code 0 on success, nonzero on error or a failed (expect …),\n"
      "so a model file is a self-checking test."};

  std::string path;
  app.add_option("model", path, "the .hsc model file")
      ->required()
      ->check(CLI::ExistingFile);

  bool dump = false;
  app.add_flag("--expand", dump,
               "stop after the parametric pass; print the flat, runnable "
               ".hsc text");

  std::vector<std::string> defines;
  app.add_option("-D", defines,
                 "override a (param NAME …) from the command line, as "
                 "NAME=VALUE; repeatable")
      ->allow_extra_args(false);

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return app.exit(e);
  }

  std::map<std::string, long long> params;
  for (const std::string& d : defines) {
    const auto eq = d.find('=');
    if (eq == std::string::npos || eq == 0) {
      std::cerr << "-D" << d << ": expected -DNAME=VALUE\n";
      return 2;
    }
    try {
      params[d.substr(0, eq)] = std::stoll(d.substr(eq + 1));
    } catch (const std::exception&) {
      std::cerr << "-D" << d << ": expected an integer value\n";
      return 2;
    }
  }

  if (dump) return dump_expanded(path, params);
  return hsc::surface::run_file(path, std::cout, std::cerr, params);
}
