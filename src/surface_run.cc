/// \file surface_run.cc
/// \brief The runner: the public `translate()`/`run_file()` entry. Routes
/// each form — explicit-engine commands (`xreach`, `xdomains`) and the
/// query overlay on explicit results are handled here; every other form
/// goes to the symbolic translator. The one layer that knows both
/// engines; neither engine knows the other.

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

#include "surface_translator.hh"

#include "hsc/cegar/loop.hh"
#include "hsc/surface/cegar_build.hh"
#include "hsc/surface/certcheck.hh"
#include "hsc/surface/expand.hh"
#include "hsc/surface/rewrite.hh"
#include "hsc/surface/spec.hh"
#include "hsc/surface/xpl_build.hh"
#include "hsc/xpl/engine.hh"

namespace hsc::surface {

namespace {

class runner {
 public:
  explicit runner(std::ostream& out) : out_(out), t_(out) {}

  void set_deadline(std::optional<std::chrono::steady_clock::time_point> at) { t_.set_deadline(at); }

  int run(const std::vector<datum>& forms) {
    // rewrite directives: forms whose head names a pass of the chain
    // ((simplify-constants) today; the vocabulary grows). Consumed here,
    // applied in file order to the whole spec, trace printed — identity
    // included, never silent.
    std::vector<pass> chain;
    std::vector<datum> work;
    const std::vector<pass_def> registry = pass_registry();
    for (const datum& f : forms) {
      const pass_def* hit = nullptr;
      if (f.is_list() && !f.items().empty()) {
        for (const pass_def& p : registry) {
          if (p.name == f.head()) hit = &p;
        }
      }
      if (hit) {
        chain.push_back({hit->name, [apply = hit->apply, directive = f](
                                        std::vector<datum> fs) {
                           return apply(std::move(fs), directive);
                         }});
      } else {
        work.push_back(f);
      }
    }
    // A cegar command wants the fragment-friendly spec: constants
    // elided, static arrays dissolved, the spine flattened — appended
    // to the chain unless the file already asked; the trace lines keep
    // it visible.
    const bool wants_cegar =
        std::any_of(work.begin(), work.end(),
                    [](const datum& f) { return f.head() == "cegar"; });
    if (wants_cegar) {
      for (const char* name :
           {"simplify-constants", "simplify-arrays", "flatten"}) {
        if (std::any_of(chain.begin(), chain.end(),
                        [&](const pass& p) { return p.name == name; }))
          continue;
        for (const pass_def& p : registry) {
          if (p.name != name) continue;
          const datum directive =
              datum::list({datum::atom(name, 0)}, 0);
          chain.push_back({p.name, [apply = p.apply, directive](
                                       std::vector<datum> fs) {
                             return apply(std::move(fs), directive);
                           }});
        }
      }
    }
    if (!chain.empty()) {
      auto [rewritten, log] = rewrite(std::move(work), chain);
      work = std::move(rewritten);
      for (const trace_entry& e : log) {
        out_ << "rewrite " << e.pass_name
             << (e.applied ? ": " : " (identity): ") << e.trace << '\n';
      }
    }
    owned_ = std::move(work);
    forms_ = &owned_;
    for (const datum& f : *forms_) route(f);
    return t_.failures() + failures_;
  }

 private:
  [[noreturn]] static void fail(const datum& d, const std::string& msg) {
    throw translate_error(d.line(), msg);
  }

  void route(const datum& f) {
    if (!f.is_list() || f.items().empty()) {
      t_.form(f);  // the translator's error wording applies
      return;
    }
    const std::string& kw = f.head();
    if (kw == "xreach") return do_xreach(f);
    if (kw == "xdomains") return do_xdomains(f);
    if (kw == "cegar") return do_cegar(f);
    if (kw == "certificate") return do_certificate(f);
    if (kw == "certcheck") return do_certcheck(f);
    if (kw == "print-spec") {
      // `(print-spec [FILE])`: the current spec, post-chain, runnable;
      // to FILE (relative to the working directory) instead of stdout
      // when named.
      std::ofstream sink;
      if (f.items().size() == 2 && f.items()[1].is_atom()) {
        sink.open(f.items()[1].text(), std::ios::binary);
        if (!sink) fail(f, "print-spec: cannot open " + f.items()[1].text());
      } else if (f.items().size() != 1) {
        fail(f, "print-spec takes at most one file name");
      }
      std::ostream& dst = sink.is_open() ? sink : out_;
      for (const datum& g : *forms_) {
        if (g.is_list() && !g.items().empty() && g.head() == "print-spec") {
          continue;  // not part of the spec it prints
        }
        write(dst, g);
        dst << '\n';
      }
      return;
    }
    // the overlay: queries whose subject is an explicit result
    if (f.items().size() > 1 && f.items()[1].is_atom() &&
        xresults_.contains(f.items()[1].text())) {
      if (kw == "count") return xcount(f);
      if (kw == "expect") return xexpect(f);
      if (kw == "get-states") return xget_states(f);
      if (kw == "get-witness") return xget_witness(f);
      if (kw == "nodes" || kw == "profile" || kw == "stock" || kw == "print" || kw == "max-value") {
        // a diagram statistic; no diagram — an honest line, not a failure
        out_ << f.items()[1].text() << ' ' << kw << " (unsupported)\n";
        return;
      }
    }
    t_.form(f);
  }

  // --- the explicit engine's side -------------------------------------------

  /// The spec view of the whole input, read once on first need.
  const spec& model_spec() {
    if (!spec_) spec_ = spec::read(*forms_);
    return *spec_;
  }

  const xpl::model& model() {
    if (!model_built_) {
      const spec& s = model_spec();
      const expr_reader reader(ex_, s);
      model_ = build_xpl_model(s.order().size(), ex_, reader, s, s.events());
      model_built_ = true;
    }
    return model_;
  }

  void print_word(const std::vector<std::int32_t>& values) {
    const auto& order = model_spec().order();
    out_ << '(';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out_ << ' ';
      out_ << '(' << order[i] << ' ' << values[i] << ')';
    }
    out_ << ')';
  }

  /// `(xreach NAME [from RESULT] [cap INT])`: close a seed set under the
  /// default system with the explicit engine. Seeds enumerate from a
  /// bound symbolic result, or default to the spec's initial states.
  void do_xreach(const datum& form) {
    if (form.items().size() < 2) fail(form, "xreach needs a result name");
    const std::string& name = form.items()[1].text();
    if (t_.has_result(name) || xresults_.contains(name)) {
      fail(form, "result '" + name + "' already bound");
    }
    std::optional<std::string> from;
    xpl::reach_options opt;
    for (std::size_t i = 2; i < form.items().size(); ++i) {
      const std::string& kw = form.items()[i].text();
      if (kw == "from" && i + 1 < form.items().size()) {
        from = form.items()[++i].text();
      } else if (kw == "cap" && i + 1 < form.items().size()) {
        opt.cap = static_cast<std::size_t>(
            std::stoll(form.items()[++i].text()));
      } else {
        fail(form.items()[i], "xreach options are: from RESULT, cap INT");
      }
    }
    std::vector<xpl::word> seeds;
    if (from) {
      auto words = t_.words_of(*from, opt.cap);
      if (!words) fail(form, "no result named '" + *from + "'");
      if (words->size() > opt.cap) {
        fail(form, "xreach: the seed set alone exceeds the cap");
      }
      seeds = std::move(*words);
    } else {
      seeds = model_spec().seeds(ex_);
    }
    xpl::reach_result res = xpl::reach(model(), seeds, opt);
    switch (res.stats.st) {
      case xpl::explore_stats::status::ok:
        out_ << name << " xreach " << res.states.size() << " states ("
             << res.stats.fired << " fired)\n";
        break;
      case xpl::explore_stats::status::capped:
        out_ << name << " xreach CAP: " << res.stats.diagnostic << '\n';
        break;
      default:  // error: TOP (reach has no visitor, it never stops early)
        out_ << name << " xreach TOP: " << res.stats.diagnostic << " at ";
        print_word(res.stats.witness);
        out_ << '\n';
        ++failures_;
        break;
    }
    xresults_.emplace(name, std::move(res));
  }

  /// `(xdomains)`: the decoration step — print the inferred per-unit
  /// domains (`spec.hh`); data only, nothing is tagged yet.
  void do_xdomains(const datum&) { print_domains(out_, analyze_domains(*forms_)); }

  // --- the cegar loop's commands --------------------------------------------

  /// `(cegar NAME QATOM+ [all|first|cheapest] [jump-exact] [cap INT])`:
  /// run the loop against bad = the atoms. Binds NAME as an explicit
  /// result — the validated bad state on violation, empty on holds (so
  /// `(expect NAME 0)` asserts "holds"). Holds retains the certificate
  /// for `(certificate FILE)`.
  void do_cegar(const datum& form) {
    if (form.items().size() < 2 || !form.items()[1].is_atom()) {
      fail(form, "cegar needs a result name");
    }
    const std::string& name = form.items()[1].text();
    if (t_.has_result(name) || xresults_.contains(name)) {
      fail(form, "result '" + name + "' already bound");
    }
    std::vector<datum> atoms;
    cegar::options opt;
    for (std::size_t i = 2; i < form.items().size(); ++i) {
      const datum& it = form.items()[i];
      if (it.is_list()) {
        atoms.push_back(it);
      } else if (it.text() == "all") {
        opt.pick = cegar::options::culprits::all;
      } else if (it.text() == "first") {
        opt.pick = cegar::options::culprits::first;
      } else if (it.text() == "cheapest") {
        opt.pick = cegar::options::culprits::cheapest;
      } else if (it.text() == "jump-exact") {
        opt.jump_exact = true;
      } else if (it.text() == "no-intern") {
        opt.intern = false;  // the ablation knob: one entry per leaf
      } else if (it.text() == "cap" && i + 1 < form.items().size()) {
        opt.cap = std::stoll(form.items()[++i].text());
      } else {
        fail(it, "cegar options are: all|first|cheapest, jump-exact, "
                 "no-intern, cap INT");
      }
    }
    if (atoms.empty()) fail(form, "cegar needs at least one property atom");
    const spec& s = model_spec();
    const expr_reader reader(ex_, s);
    const cegar_bridge b = build_cegar_model(s, ex_, reader, atoms);
    const cegar::run_result r = cegar::run(b.model, opt);
    if (r.v.k == cegar::verdict::kind::cap) {
      fail(form, "cegar: abstract search exceeded its cap (" +
                     std::to_string(r.v.states_walked) +
                     " abstract states materialized; the abstraction "
                     "diverged — raise `cap N` or coarsen the "
                     "decomposition)");
    }
    const bool holds = r.v.k == cegar::verdict::kind::holds;
    out_ << name << " cegar " << (holds ? "holds" : "violation")
         << " rounds " << r.rounds << " cex " << r.cex_total << '/'
         << r.budget << " rungs " << r.leaves_chaotic << '/'
         << r.leaves_intermediate << '/' << r.leaves_exact << " inv "
         << r.inv_size << " abstract " << r.v.states_walked << " ns "
         << r.ns_search << '/' << r.ns_replay << '/' << r.ns_refine << '\n';
    xpl::reach_result res{{}, xpl::state_store(s.order().size())};
    res.stats.st = xpl::explore_stats::status::ok;
    if (holds) {
      last_cert_ = r.certificate;
    } else {
      out_ << name << " witness";
      for (std::int32_t e : r.v.witness)
        out_ << ' ' << b.model.events[static_cast<std::size_t>(e)].name;
      out_ << '\n';
      const xpl::word w(r.final_values.begin(), r.final_values.end());
      (void)res.states.intern(xpl::state_view{w.data(), w.size()});
    }
    res.stats.count = res.states.size();
    xresults_.emplace(name, std::move(res));
  }

  /// `(certificate FILE)`: write the certificate retained by the last
  /// holding `cegar` run.
  void do_certificate(const datum& form) {
    if (form.items().size() != 2 || !form.items()[1].is_atom()) {
      fail(form, "certificate needs one file name");
    }
    if (last_cert_.empty()) {
      // Not a failure: linear scripts run the same driver on both
      // verdicts; the skip is printed, never silent.
      out_ << "certificate " << form.items()[1].text()
           << " skipped (no proof retained)\n";
      return;
    }
    const std::string& path = form.items()[1].text();
    std::ofstream f(path, std::ios::binary);
    if (!f) fail(form, "cannot write " + path);
    f << last_cert_;
    out_ << "certificate " << path << " (" << last_cert_.size()
         << " bytes)\n";
  }

  /// `(certcheck FILE)`: the trusted core — check a certificate document
  /// against the current spec.
  void do_certcheck(const datum& form) {
    if (form.items().size() != 2 || !form.items()[1].is_atom()) {
      fail(form, "certcheck needs one file name");
    }
    const std::string& path = form.items()[1].text();
    std::ifstream f(path, std::ios::binary);
    if (!f) fail(form, "cannot read " + path);
    std::ostringstream buf;
    buf << f.rdbuf();
    const std::vector<datum> forms = parse(buf.str());
    const spec& s = model_spec();
    const expr_reader reader(ex_, s);
    failures_ += certcheck(s, ex_, reader, forms, out_);
  }

  // --- the overlay on explicit results --------------------------------------

  void xcount(const datum& form) {
    const auto& res = xresults_.at(form.items()[1].text());
    if (res.stats.st == xpl::explore_stats::status::ok) {
      out_ << form.items()[1].text() << " count " << res.states.size()
           << '\n';
    } else {
      out_ << form.items()[1].text() << " count TOP\n";
      ++failures_;
    }
  }

  void xexpect(const datum& form) {
    const std::string& name = form.items()[1].text();
    if (form.items().size() < 3) fail(form, "expect needs a count");
    const auto want = std::stoull(form.items()[2].text());
    const auto& res = xresults_.at(name);
    if (res.stats.st == xpl::explore_stats::status::ok &&
        res.states.size() == want) {
      out_ << "ok " << name << " == " << want << '\n';
    } else if (res.stats.st == xpl::explore_stats::status::ok) {
      out_ << "FAIL " << name << " expected " << want << " got "
           << res.states.size() << '\n';
      ++failures_;
    } else {
      out_ << "FAIL " << name << " expected " << want << " got TOP ("
           << res.stats.diagnostic << ")\n";
      ++failures_;
    }
  }

  void xget_states(const datum& form) {
    const std::string& name = form.items()[1].text();
    std::size_t limit = 10;
    if (form.items().size() > 2) {
      limit = static_cast<std::size_t>(std::stoll(form.items()[2].text()));
    }
    const auto& st = xresults_.at(name).states;
    out_ << name << ' ' << st.size() << " states, showing up to " << limit
         << '\n';
    std::vector<std::int32_t> w;
    for (std::size_t i = 0; i < st.size() && i < limit; ++i) {
      const auto v = st[static_cast<xpl::state_id>(i)];
      w.assign(v.begin(), v.end());
      print_word(w);
      out_ << '\n';
    }
  }

  void xget_witness(const datum& form) {
    const std::string& name = form.items()[1].text();
    const auto& st = xresults_.at(name).states;
    out_ << name << " witness ";
    if (st.size() == 0) {
      out_ << "none";
    } else {
      const auto v = st[0];
      print_word(std::vector<std::int32_t>(v.begin(), v.end()));
    }
    out_ << '\n';
  }

  std::ostream& out_;
  translator t_;
  std::vector<datum> owned_;  ///< the forms after the rewrite chain
  const std::vector<datum>* forms_ = nullptr;
  std::optional<spec> spec_;
  lia::expr_factory ex_;
  xpl::model model_;
  bool model_built_ = false;
  std::unordered_map<std::string, xpl::reach_result> xresults_;
  std::string last_cert_;  ///< retained by the last holding cegar run
  int failures_ = 0;
};

}  // namespace

int translate(const std::vector<datum>& forms, std::ostream& out) {
  runner r(out);
  return r.run(forms);
}

struct session::impl {
  explicit impl(std::ostream& out) : r(out) {}
  runner r;
};

session::session(std::ostream& out) : impl_(std::make_unique<impl>(out)) {}
session::~session() = default;

int session::feed(const std::vector<datum>& forms) {
  return impl_->r.run(forms);
}

void session::set_deadline(std::optional<std::chrono::steady_clock::time_point> at) {
  impl_->r.set_deadline(at);
}

namespace {

/// `(input FILE)`: splice the forms of FILE in place — the model/script
/// split. Relative paths resolve against the including file; cycles and
/// unreadable files are parse errors naming the culprit.
std::vector<datum> splice_inputs(std::vector<datum> forms,
                                 const std::filesystem::path& dir,
                                 std::vector<std::filesystem::path>& stack) {
  std::vector<datum> out;
  out.reserve(forms.size());
  for (datum& f : forms) {
    if (f.head() != "input") {
      out.push_back(std::move(f));
      continue;
    }
    if (f.items().size() != 2 || !f.items()[1].is_atom()) {
      throw parse_error(f.line(), "input takes one file name");
    }
    std::filesystem::path p(f.items()[1].text());
    if (p.is_relative()) p = dir / p;
    std::error_code ec;
    const std::filesystem::path canon = std::filesystem::weakly_canonical(p, ec);
    if (!ec && std::find(stack.begin(), stack.end(), canon) != stack.end()) {
      throw parse_error(f.line(), "input cycle through " + canon.string());
    }
    std::ifstream in(p, std::ios::binary);
    if (!in) throw parse_error(f.line(), "cannot open input " + p.string());
    std::ostringstream buf;
    buf << in.rdbuf();
    stack.push_back(canon);
    std::vector<datum> sub =
        splice_inputs(parse(buf.str()), p.parent_path(), stack);
    stack.pop_back();
    for (datum& g : sub) out.push_back(std::move(g));
  }
  return out;
}

}  // namespace

int run_file(const std::string& path, std::ostream& out, std::ostream& err,
             const std::map<std::string, long long>& params) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    err << "cannot open " << path << '\n';
    return 2;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  const std::string text = buf.str();
  try {
    std::vector<std::filesystem::path> stack{
        std::filesystem::weakly_canonical(path)};
    const std::vector<datum> forms =
        expand(splice_inputs(parse(text),
                             std::filesystem::path(path).parent_path(), stack),
               /*families=*/true, params);
    return translate(forms, out) == 0 ? 0 : 1;
  } catch (const parse_error& e) {
    err << path << ": parse error: " << e.what() << '\n';
    return 2;
  } catch (const expand_error& e) {
    err << path << ": expand error: " << e.what() << '\n';
    return 2;
  } catch (const translate_error& e) {
    err << path << ": " << e.what() << '\n';
    return 2;
  } catch (const std::exception& e) {
    // Not one of the language's own error kinds: an internal defect.
    // Loud and named, never a core dump.
    err << path << ": internal error: " << e.what() << '\n';
    return 3;
  }
}

int run_session(const std::vector<session_arg>& args, std::ostream& out,
                std::ostream& err,
                const std::map<std::string, long long>& params) {
  // The root session: each file argument as an `(input PATH)` form, each
  // inline argument as its parsed forms, in invocation order. From there
  // the pipeline is `run_file`'s: splice, expand, translate.
  std::vector<datum> root;
  std::size_t nth = 0;
  for (const session_arg& a : args) {
    if (a.is_file) {
      root.push_back(
          datum::list({datum::atom("input", 0), datum::atom(a.text, 0)}, 0));
      continue;
    }
    ++nth;
    try {
      for (datum& f : parse(a.text)) root.push_back(std::move(f));
    } catch (const parse_error& e) {
      err << "-e #" << nth << ": parse error: " << e.what() << '\n';
      return 2;
    }
  }
  const std::string label =
      args.size() == 1 && args.front().is_file ? args.front().text : "session";
  try {
    std::vector<std::filesystem::path> stack;
    const std::vector<datum> forms =
        expand(splice_inputs(std::move(root),
                             std::filesystem::current_path(), stack),
               /*families=*/true, params);
    return translate(forms, out) == 0 ? 0 : 1;
  } catch (const parse_error& e) {
    err << label << ": parse error: " << e.what() << '\n';
    return 2;
  } catch (const expand_error& e) {
    err << label << ": expand error: " << e.what() << '\n';
    return 2;
  } catch (const translate_error& e) {
    err << label << ": " << e.what() << '\n';
    return 2;
  } catch (const std::exception& e) {
    err << label << ": internal error: " << e.what() << '\n';
    return 3;
  }
}

}  // namespace hsc::surface
