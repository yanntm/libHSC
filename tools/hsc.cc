/// \file hsc.cc
/// \brief The `hsc` CLI: run a session of `.hsc` files and inline forms —
/// parse, expand, translate, execute.
///
/// The invocation grammar is sugar over the language's own `input`: each
/// positional FILE behaves exactly as `(input FILE)` and each `-e FORMS`
/// as the forms themselves, spliced in command-line order into one
/// session. Everything else — engine choice, rewriting, printing — is
/// said in the language (see doc/hsc_manual.md). Exit code is 0 on
/// success, nonzero on an error or a failed `expect`, so a session is a
/// self-checking test.

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "hsc/surface/translate.hh"

int main(int argc, char** argv) {
  CLI::App app{
      "hsc — run a .hsc session: parse, expand, translate, execute.\n"
      "Positional files and -e forms splice in command-line order, each\n"
      "file exactly as (input FILE) would. Exit code 0 on success,\n"
      "nonzero on error or a failed (expect …), so a session is a\n"
      "self-checking test."};

  std::vector<std::string> files;
  CLI::Option* file_opt =
      app.add_option("model", files, "a .hsc file, spliced at this position")
          ->check(CLI::ExistingFile);

  std::vector<std::string> inlines;
  CLI::Option* e_opt =
      app.add_option("-e", inlines,
                     "inline .hsc forms, spliced at this position; "
                     "repeatable, may sit between files")
          ->allow_extra_args(false);

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

  // Rebuild the session in invocation order: parse_order records one
  // entry per parsed occurrence, and each option's values are stored in
  // that same order.
  std::vector<hsc::surface::session_arg> args;
  std::size_t nf = 0;
  std::size_t ne = 0;
  for (const CLI::Option* o : app.parse_order()) {
    if (o == file_opt) {
      args.push_back({true, files[nf++]});
    } else if (o == e_opt) {
      args.push_back({false, inlines[ne++]});
    }
  }
  if (args.empty()) {
    std::cerr << "nothing to run: give a .hsc file or -e forms\n";
    return 2;
  }
  return hsc::surface::run_session(args, std::cout, std::cerr, params);
}
