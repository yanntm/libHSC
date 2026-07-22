# libHSC

**libHSC** is a library for the **Hierarchical Shape Calculus** (HSC): symbolic
representation and transformation of very large structured state spaces, with
hierarchy as the primitive. It is the concepts-first reboot of
[libDDD](https://github.com/lip6/libDDD) / [libITS](https://github.com/lip6/libITS):
hierarchical decision diagrams and `split_equiv` (partition queries with
discovered alphabets) form the base of the calculus, while flat decision
diagrams and plain homomorphisms are recognized, fast degenerations.

## Layout

- [`research_notes/`](research_notes/) — the calculus drafts (`hsc_core*.md`,
  highest number is current) and the bridge document
  [`ddd_to_hsc.md`](research_notes/ddd_to_hsc.md) (goals, correspondence with
  the legacy code bases, milestones).
- [`python/hsc/`](python/hsc/) — a Python prototype of the calculus: API
  shape, cost model, and pressure on the theory's open obligations.
  Performance is not its goal.
- The C++ core (C++20/23) will live in `include/hsc/` and `src/` once the
  theory interface is settled.

> This material is **unpublished** work in progress. Feedback and
> collaboration are welcome — contact **Yann.Thierry-Mieg@lip6.fr** or open a
> GitHub issue.

## License

Distributed under the **GNU General Public License v3.0** (see
[`LICENSE`](LICENSE)).

© 2026 Yann Thierry-Mieg, LIP6, Sorbonne Université, CNRS.
