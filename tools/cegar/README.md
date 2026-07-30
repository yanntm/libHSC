# `tools/cegar/` — the CEGAR binaries

* `hsc-cegar.cc` — the driver, linking `hsc_cegar`:
  * `hsc-cegar run model.cts [--policy all|first|cheapest] [--jump-exact]
    [--no-intern] [--cap N] [--cert out.cert] [--tsv]` — the loop;
    prints the verdict and stats (or one TSV row with `--tsv`).
  * `hsc-cegar mono model.cts [--cap N]` — the monolithic oracle walk.
  * `hsc-cegar gen FAMILY ARGS... [-o out.cts]` — model families:
    `clients K`, `clients-bug K`, `ring N`, `rand L Q S E D SEED`.
* `hsc-certcheck.cc` — the independent certificate checker:
  `hsc-certcheck model.cts proof.cert`. Exit 0 iff every obligation
  passes. **Deliberately shares no code with the library**: its own
  parsers and walks in one self-contained translation unit — the
  trusted core must not inherit a bug from the code it checks.
