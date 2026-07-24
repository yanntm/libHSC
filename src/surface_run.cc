/// \file surface_run.cc
/// \brief The runner: the public `translate()`/`run_file()` entry. Routes
/// each form — explicit-engine commands (`xreach`, `xdomains`) and the
/// query overlay on explicit results are handled here; every other form
/// goes to the symbolic translator. The one layer that knows both
/// engines; neither engine knows the other.

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

#include "surface_translator.hh"

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
    if (kw == "print-spec") {  // the current spec, post-chain, runnable
      for (const datum& g : *forms_) {
        if (g.is_list() && !g.items().empty() && g.head() == "print-spec") {
          continue;  // not part of the spec it prints
        }
        write(out_, g);
        out_ << '\n';
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
      if (kw == "nodes" || kw == "print" || kw == "max-value") {
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
  int failures_ = 0;
};

}  // namespace

int translate(const std::vector<datum>& forms, std::ostream& out) {
  runner r(out);
  return r.run(forms);
}

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
    const std::vector<datum> forms =
        expand(parse(text), /*families=*/true, params);
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
  }
}

}  // namespace hsc::surface
