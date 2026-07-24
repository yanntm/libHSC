/// \file hsc.cc
/// \brief The `hsc` CLI: run a model from a `.hsc` file — parse, expand,
/// translate, execute, report.
///
/// The whole surface behind one entry point. Exit code is 0 on success,
/// nonzero on a parse/expand/translate error or a failed `expect` — so a
/// model file is a self-checking test. `--expand` stops after the parametric
/// pass and prints the expanded model as plain `.hsc` text: the
/// degeneralization of a parametric file, itself runnable. `--explicit`
/// runs the same file on the explicit engine instead: each `(reach NAME …)`
/// becomes `(xreach NAME …)`, and the file's own `count`/`expect` read the
/// explicit result — output lines match the symbolic run, so a corpus
/// differential is a diff of outputs.

#include <fstream>
#include <iostream>
#include <functional>
#include <map>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "hsc/surface/expand.hh"
#include "hsc/surface/rewrite.hh"
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

/// `--explicit`: `(reach NAME [saturate|naive] [from R])` becomes
/// `(xreach NAME [from R] [cap K])`. A `reach` carrying an event term is
/// refused: the explicit engine runs the default system.
std::vector<hsc::surface::datum> to_explicit(
    std::vector<hsc::surface::datum> forms, long long cap) {
  using hsc::surface::datum;
  std::vector<datum> out;
  out.reserve(forms.size());
  for (datum& f : forms) {
    if (!f.is_list() || f.items().empty() || f.head() != "reach" ||
        f.items().size() < 2) {
      out.push_back(std::move(f));
      continue;
    }
    std::vector<datum> items;
    items.push_back(datum::atom("xreach", f.line()));
    items.push_back(f.items()[1]);  // the result name
    for (std::size_t i = 2; i < f.items().size(); ++i) {
      const datum& d = f.items()[i];
      if (d.is_atom() && (d.text() == "saturate" || d.text() == "naive")) {
        continue;  // a strategy of the symbolic engine
      }
      if (d.is_atom() && d.text() == "from" && i + 1 < f.items().size()) {
        items.push_back(d);
        items.push_back(f.items()[++i]);
        continue;
      }
      throw std::runtime_error(
          "line " + std::to_string(f.line()) +
          ": --explicit supports only the default system; this (reach …) "
          "names an event term");
    }
    if (cap > 0) {
      items.push_back(datum::atom("cap", f.line()));
      items.push_back(datum::atom(std::to_string(cap), f.line()));
    }
    out.push_back(datum::list(std::move(items), f.line()));
  }
  return out;
}

/// `--domains`: keep only the declaration forms (leaf, array, shape,
/// init, event, family, alt, seq), then run the domain-inference
/// decoration — no reach of either engine.
std::vector<hsc::surface::datum> to_domains(
    std::vector<hsc::surface::datum> forms) {
  using hsc::surface::datum;
  static const std::set<std::string> kept = {
      "leaf", "array", "shape", "init", "event", "family", "alt", "seq"};
  std::vector<datum> out;
  int line = 1;
  for (datum& f : forms) {
    if (f.is_list() && !f.items().empty() && kept.contains(f.head())) {
      line = f.line();
      out.push_back(std::move(f));
    }
  }
  out.push_back(datum::list({datum::atom("xdomains", line)}, line));
  return out;
}

/// `--rewrite`: run the default rewrite chain, print the rewritten spec
/// as runnable .hsc text; traces go to stderr.
int dump_rewritten(const std::string& path,
                   const std::map<std::string, long long>& params) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    std::cerr << "cannot open " << path << '\n';
    return 2;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  try {
    const std::vector<hsc::surface::pass> chain = hsc::surface::default_chain();
    auto [forms, log] = hsc::surface::rewrite(
        hsc::surface::expand(hsc::surface::parse(buf.str()),
                             /*families=*/true, params),
        chain);
    for (const hsc::surface::trace_entry& e : log) {
      std::cerr << "rewrite " << e.pass_name
                << (e.applied ? ": " : " (identity): ") << e.trace << '\n';
    }
    for (const hsc::surface::datum& form : forms) {
      hsc::surface::write(std::cout, form);
      std::cout << '\n';
    }
    return 0;
  } catch (const hsc::surface::parse_error& e) {
    std::cerr << path << ": parse error: " << e.what() << '\n';
  } catch (const hsc::surface::expand_error& e) {
    std::cerr << path << ": expand error: " << e.what() << '\n';
  } catch (const hsc::surface::translate_error& e) {
    std::cerr << path << ": " << e.what() << '\n';
  }
  return 2;
}

/// Parse, expand, rewrite by \p tf, translate. The shared scaffolding of
/// the `--explicit` and `--domains` modes.
int run_transformed(
    const std::string& path, const std::map<std::string, long long>& params,
    const std::function<std::vector<hsc::surface::datum>(
        std::vector<hsc::surface::datum>)>& tf) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    std::cerr << "cannot open " << path << '\n';
    return 2;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  try {
    const std::vector<hsc::surface::datum> forms =
        tf(hsc::surface::expand(hsc::surface::parse(buf.str()),
                                /*families=*/true, params));
    return hsc::surface::translate(forms, std::cout) == 0 ? 0 : 1;
  } catch (const hsc::surface::parse_error& e) {
    std::cerr << path << ": parse error: " << e.what() << '\n';
  } catch (const hsc::surface::expand_error& e) {
    std::cerr << path << ": expand error: " << e.what() << '\n';
  } catch (const hsc::surface::translate_error& e) {
    std::cerr << path << ": " << e.what() << '\n';
  } catch (const std::runtime_error& e) {
    std::cerr << path << ": " << e.what() << '\n';
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

  bool use_explicit = false;
  app.add_flag("--explicit", use_explicit,
               "run on the explicit engine: each (reach NAME ...) becomes "
               "(xreach NAME ...); count/expect read the explicit result");

  bool do_rewrite = false;
  app.add_flag("--rewrite", do_rewrite,
               "run the rewrite chain (simplify-constants, ...); print the "
               "rewritten spec, traces on stderr");

  bool use_domains = false;
  app.add_flag("--domains", use_domains,
               "domain inference only: keep the declarations, run "
               "(xdomains), print one xdom line per unit");

  long long cap = 0;
  app.add_option("--cap", cap,
                 "with --explicit: bound on stored states (default 10^8)")
      ->check(CLI::PositiveNumber);

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
  if (do_rewrite) return dump_rewritten(path, params);
  if (use_domains) return run_transformed(path, params, to_domains);
  if (use_explicit) {
    return run_transformed(path, params, [cap](auto forms) {
      return to_explicit(std::move(forms), cap);
    });
  }
  return hsc::surface::run_file(path, std::cout, std::cerr, params);
}
