/// \file hsc-cegar.cc — CLI driver for the CEGAR package: run the loop,
/// run the monolithic oracle, generate model families.

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "hsc/cegar/gen.hh"
#include "hsc/cegar/loop.hh"
#include "hsc/cegar/model.hh"

namespace {

using namespace hsc::cegar;

int usage() {
  std::fprintf(stderr,
               "usage: hsc-cegar run model.cts [--policy all|first|cheapest]"
               " [--jump-exact] [--no-intern] [--cap N] [--cert F] [--tsv]\n"
               "       hsc-cegar mono model.cts [--cap N]\n"
               "       hsc-cegar gen clients K | clients-bug K | ring N |"
               " rand L Q S E D SEED  [-o out.cts]\n");
  return 2;
}

std::optional<model> load(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    std::fprintf(stderr, "cannot open %s\n", path.c_str());
    return std::nullopt;
  }
  std::stringstream ss;
  ss << in.rdbuf();
  std::string err;
  auto m = parse_cts(ss.str(), &err);
  if (!m) std::fprintf(stderr, "%s: %s\n", path.c_str(), err.c_str());
  return m;
}

const char* kind_name(verdict::kind k) {
  switch (k) {
    case verdict::kind::holds: return "holds";
    case verdict::kind::violation: return "violation";
    default: return "cap";
  }
}

int cmd_run(int argc, char** argv) {
  if (argc < 1) return usage();
  auto m = load(argv[0]);
  if (!m) return 1;
  options opt;
  std::string cert_path;
  bool tsv = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--policy" && i + 1 < argc) {
      std::string p = argv[++i];
      if (p == "all") opt.pick = options::culprits::all;
      else if (p == "first") opt.pick = options::culprits::first;
      else if (p == "cheapest") opt.pick = options::culprits::cheapest;
      else return usage();
    } else if (a == "--jump-exact") {
      opt.jump_exact = true;
    } else if (a == "--no-intern") {
      opt.intern = false;
    } else if (a == "--cap" && i + 1 < argc) {
      opt.cap = std::atoll(argv[++i]);
    } else if (a == "--cert" && i + 1 < argc) {
      cert_path = argv[++i];
    } else if (a == "--tsv") {
      tsv = true;
    } else {
      return usage();
    }
  }
  run_result r = run(*m, opt);
  if (r.v.k == verdict::kind::violation && !refire(*m, r.v.witness)) {
    std::fprintf(stderr, "INTERNAL: violation witness does not refire\n");
    return 3;
  }
  if (tsv) {
    std::cout << kind_name(r.v.k) << "\t" << r.rounds << "\t" << r.cex_total
              << "\t" << r.budget << "\t" << r.leaves_chaotic << "\t"
              << r.leaves_intermediate << "\t" << r.leaves_exact << "\t"
              << r.inv_size << "\t" << r.v.states_walked << "\t"
              << r.v.witness.size() << "\n";
  } else {
    std::cout << "verdict: " << kind_name(r.v.k) << "\n"
              << "rounds: " << r.rounds << "  counterexamples: " << r.cex_total
              << " / budget " << r.budget << "\n"
              << "leaves chaotic/intermediate/exact: " << r.leaves_chaotic
              << "/" << r.leaves_intermediate << "/" << r.leaves_exact << "\n";
    if (r.v.k == verdict::kind::holds)
      std::cout << "|Inv|: " << r.inv_size << "\n";
    if (r.v.k == verdict::kind::violation) {
      std::cout << "witness:";
      for (auto e : r.v.witness) std::cout << " " << m->events[e].name;
      std::cout << "\nwitness refires concretely: yes\n";
    }
  }
  if (!cert_path.empty() && r.v.k == verdict::kind::holds) {
    std::ofstream out(cert_path);
    out << r.certificate;
  }
  return 0;
}

int cmd_mono(int argc, char** argv) {
  if (argc < 1) return usage();
  auto m = load(argv[0]);
  if (!m) return 1;
  std::int64_t cap = 1'000'000;
  for (int i = 1; i < argc; ++i)
    if (std::string(argv[i]) == "--cap" && i + 1 < argc)
      cap = std::atoll(argv[++i]);
  verdict v = mono(*m, cap);
  std::cout << kind_name(v.k) << "\t" << v.states_walked << "\t"
            << v.witness.size() << "\n";
  return 0;
}

int cmd_gen(int argc, char** argv) {
  if (argc < 2) return usage();
  std::string fam = argv[0];
  model m;
  std::string params;
  int used = 0;
  if (fam == "clients" || fam == "clients-bug" || fam == "ring") {
    std::int32_t k = std::atoi(argv[1]);
    used = 2;
    params = std::to_string(k);
    m = fam == "clients"       ? gen_clients(k)
        : fam == "clients-bug" ? gen_clients_bug(k)
                               : gen_ring(k);
  } else if (fam == "rand" && argc >= 7) {
    std::int32_t l = std::atoi(argv[1]), q = std::atoi(argv[2]),
                 s = std::atoi(argv[3]), e = std::atoi(argv[4]);
    double d = std::atof(argv[5]);
    std::uint64_t seed = std::strtoull(argv[6], nullptr, 10);
    used = 7;
    params = std::string(argv[1]) + " " + argv[2] + " " + argv[3] + " " +
             argv[4] + " " + argv[5] + " " + argv[6];
    m = gen_rand(l, q, s, e, d, seed);
  } else {
    return usage();
  }
  std::string out_path;
  for (int i = used; i < argc; ++i)
    if (std::string(argv[i]) == "-o" && i + 1 < argc) out_path = argv[++i];
  std::string text =
      "# gen " + fam + " " + params + "\n" + print_cts(m);
  if (out_path.empty()) {
    std::cout << text;
  } else {
    std::ofstream out(out_path);
    out << text;
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) return usage();
  std::string cmd = argv[1];
  if (cmd == "run") return cmd_run(argc - 2, argv + 2);
  if (cmd == "mono") return cmd_mono(argc - 2, argv + 2);
  if (cmd == "gen") return cmd_gen(argc - 2, argv + 2);
  return usage();
}
