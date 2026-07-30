# CEGAR paper — citation wishlist

What v3 wants to cite, by role. Status tags: **[BtL]** = in
`~/git/BuchiToLTL/papers/` (read before citing, but the source is on
disk); **[lib]** = in `libHSC/papers/`; **[TBD: check from source]**
= cited from memory — title/venue/claim unverified, acquire the PDF
into `papers/` and read before any citation lands in the paper.

## Imports (§2) — the three pillars

* Angluin 1987, *Learning regular sets from queries and
  counterexamples* — MAT, L\* (Fact 2.4). **[BtL]**
  (`Angluin_1987_IC`)
* Rivest & Schapire 1993, *Inference of finite automata using homing
  sequences* — counterexample handling (Fact 2.4). **[BtL]**
  (`Rivest_Schapire_1993_IC`)
* Arnold 1985 TCS — synchronized products / the Arnold–Nivat
  composition (Prop 1.6). **[BtL]** (`Arnold_1985_TCS`; verify this
  is the right Arnold paper for the product, else acquire the
  Arnold–Nivat reference proper — partially from memory).
* Alpern & Schneider 1985, *Defining liveness* — safety as bad
  prefixes (Def 1.7). **[BtL]** (`Alpern_Schneider_1985_IPL`)
* Myhill–Nerode: standard; cite a textbook — Perrin & Pin 2004 or
  Pin's MPRI notes. **[BtL]** (`Perrin_Pin_2004_Book`,
  `Pin_2025_MPRI_Notes`)
* Vaandrager 2017, *Model learning* (CACM survey) — framing for
  white-box vs black-box teachers (§3). **[BtL]**
  (`Vaandrager_2017_CACM`)

## CEGAR and lazy abstraction (§0, §4 related work)

* Clarke, Grumberg, Jha, Lu, Veith — CEGAR (CAV 2000 / JACM 2003).
  **[TBD: check from source]**
* Henzinger, Jhala, Majumdar, Sutre — lazy abstraction (POPL 2002);
  our emergent cone vs their on-demand refinement. **[TBD: check
  from source]**
* Graf & Saïdi 1997 — predicate abstraction (the refiner we replace
  with a learner). **[TBD: check from source]**

## Learned assume-guarantee — the framing comparison (§9)

* Cobleigh, Giannakopoulou, Păsăreanu — *Learning assumptions for
  compositional verification* (TACAS 2003). **[TBD: check from
  source]**
* Cobleigh, Avrunin, Clarke — *Breaking up is hard to do* (ISSTA
  2006 / TOSEM 2008) — the negative result §9 answers. **[TBD:
  check from source]**
* Păsăreanu et al. — the FMSD 2008 journal treatment of learned AG.
  **[TBD: check from source]**
* Farzan, Chen, Clarke, Tsay, Wang — *Extending automated
  compositional verification to the full class of ω-regular
  languages* (TACAS 2008). **[BtL]**
  (`Farzan_Chen_Clarke_etal_2008_TACAS`)
* Chen et al. — *Learning minimal separating DFAs for compositional
  verification* (TACAS 2009). **[TBD: check from source]** (check
  whether `Chen_Liu_2017_TACAS` in BtL is a relevant successor —
  identify from source.)
* Nam & Alur — symbolic compositional verification by learning
  assumptions (CAV 2005). **[TBD: check from source]** (optional)
* AG origins one-liner: Misra & Chandy 1981; Jones 1983; Pnueli
  1985; Grumberg & Long 1994. **[TBD: check from source]** (may
  compress to one textbook pointer.)

## Symmetry and interning (Remark 5.4 related work)

* Ip & Dill 1996 — symmetry reduction in Murφ. **[BtL]**
  (`Ip_Dill_1996_FMSD`)
* Clarke, Enders, Filkorn, Jha 1996 — symmetry in temporal-logic
  model checking. **[BtL]** (`Clarke_Enders_Filkorn_Jha_1996_FMSD`)
* Emerson & Sistla 1996 — symmetry and model checking. **[BtL]**
  (`Emerson_Sistla_1996_FMSD`)

## Well-structured leaves (§7)

* Abdulla, Čerāns, Jonsson, Tsay — general decidability theorems
  (LICS 1996 / IC 2000): backward coverability. **[TBD: check from
  source]** (`Abdulla_etal_2011_CONCUR` in BtL is a different
  paper — verify, likely Ramsey-based inclusion, not this.)
* Finkel & Schnoebelen 2001 — *Well-structured transition systems
  everywhere!* **[TBD: check from source]**

## The ω deferral (§2.6, §10.3)

* Maler & Staiger 1997 — syntactic congruences of ω-languages.
  **[BtL]** (`Maler_Staiger_1997_TCS`)
* Calbrix, Nivat, Podelski 1993 — the L_$ representation. **[BtL]**
  (`Calbrix_Nivat_Podelski_1993_MFPS`)
* Maler & Pnueli 1995 — learnability of infinitary regular sets.
  **[BtL]** (`Maler_Pnueli_1995_IC`)
* Angluin & Fisman 2016/2021; Angluin, Boker, Fisman 2018 — learning
  ω-regular via families of DFAs. **[BtL]**
* Bohn & Löding 2021/2022 — active learning of ω-automata. **[BtL]**

## Symbolic walks deferral (§10.4) and house baselines (§9)

* Bryant 1986; Burch, Clarke et al. 1992 — BDDs, symbolic MC
  context. **[BtL]**
* Ciardo, Lüttgen, Siminiceanu 2001 — saturation. **[BtL]**
  (`Ciardo_Luttgen_Siminiceanu_2001_TACAS`)
* Thierry-Mieg et al. 2009 — hierarchical set decision diagrams.
  **[BtL]** (`ThierryMieg_etal_2009-TACAS`)
* Thierry-Mieg 2021 — symbolic + structural MC, ITS-tools (the house
  symbolic baseline of §9). **[lib]**
  (`thierrymieg2021_symbolic_structural_mc`)

## Comparator abstraction disciplines on shared corpora (§9)

* Berthomieu et al. 2018 — counting-marking reductions. **[lib]**
* Amat et al. 2022 — polyhedral abstraction + SMT; concurrent
  places. **[lib]** (both `amat2022_*`)
* Laarman 2018 — stubborn transaction reduction (POR comparator,
  optional). **[lib]**

## Evaluation infrastructure (§9)

* Pelánek 2007 — BEEM benchmark suite (SPIN workshop). **[TBD:
  check from source]**
* Kordon et al. — the Model Checking Contest reports (corpus + NUPN
  format). **[TBD: check from source]**
* Holzmann — SPIN; Kant et al. 2015 — LTSmin. **[TBD: check from
  source]** (only if external explicit checkers are named at all —
  §9 currently keeps them as reference points, not baselines.)

## Learner engineering (only if the implementation section warrants)

* Kearns & Vazirani 1994 — the discrimination-tree presentation.
  **[TBD: check from source]**
* Isberner, Howar, Steffen — TTT (RV 2014) and LearnLib (CAV 2015).
  **[TBD: check from source]**
