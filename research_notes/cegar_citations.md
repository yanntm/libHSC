# CEGAR paper — citation registry

Keys as used in `cegar_certified_abstractions_v3.md`. Every **verified**
entry was identified from the source text in a local archive: `papers/`
in this repository unless marked **(BtL)** = `~/git/BuchiToLTL/papers/`
(read-only holdings). Bibliographic data below is transcribed from the
source, not from memory; fields we could not confirm from the text in
hand are omitted rather than guessed. Entries under "Still to acquire"
must not be cited.

## Resolved identifications (were flagged; now settled from source)

* `Arnold_1985_TCS` (BtL) = *A syntactic congruence for rational
  ω-languages*, TCS 39:333–335 — **not** the synchronized product; it
  belongs to the ω line (§2.6/§10.3). The product cite is [Arn82] with
  [AN80]: Arnold 1982 §2.1 defines Nivat's S-synchronization exactly in
  our form — vectors of per-component actions, ε-component = the
  process waits (our skip), behaviours = ∩ of components with π(S*).
* `Abdulla_etal_2011_CONCUR` (BtL) = *Advanced Ramsey-based Büchi
  automata inclusion testing* — as suspected, unrelated to WSTS
  coverability; the §7 cites are [AČJT96, AČJT00], now acquired.
* `Chen_Liu_2017_TACAS` (BtL) is actually Li, Chen, Zhang, Liu — FDFA
  learning for Büchi automata via classification trees; a successor in
  the ω-learning line (§10.3), keyed [LCZL17], not part of the
  separating-DFA line.

## Verified — imports (§1–§2)

* **[Ang87]** D. Angluin. *Learning regular sets from queries and
  counterexamples.* Information and Computation 75:87–106, 1987.
  (BtL `Angluin_1987_IC`) — Fact 2.4.
* **[RS93]** R. L. Rivest, R. E. Schapire. *Inference of finite
  automata using homing sequences.* Information and Computation, 1993.
  (BtL `Rivest_Schapire_1993_IC`) — counterexample handling, Fact 2.4.
* **[AN80]** A. Arnold, M. Nivat. *Controlling behaviours of systems:
  some basic concepts and some applications.* 1980.
  (`Arnold_Nivat_1980_AI`) — the controls-of-processes framework
  Arnold 1982 builds on; Prop 1.6.
* **[Arn82]** A. Arnold. *Synchronized behaviours of processes and
  rational relations.* Acta Informatica 17:21–29, 1982.
  (`Arnold_1982_AI`) — S-synchronization = our product; Prop 1.6.
* **[AS85]** B. Alpern, F. B. Schneider. *Defining liveness.*
  Information Processing Letters 21(4):181–185, 1985.
  (BtL `Alpern_Schneider_1985_IPL`) — safety as bad prefixes, Def 1.7.
* **[PP04]** D. Perrin, J.-É. Pin. *Infinite Words: Automata,
  Semigroups, Logic and Games.* Elsevier Academic Press, 2004.
  (BtL `Perrin_Pin_2004_Book`) — Myhill–Nerode textbook cite, Fact 2.1.
* **[Vaa17]** F. Vaandrager. *Model learning.* Communications of the
  ACM, 2017. (BtL `Vaandrager_2017_CACM`) — black-box vs white-box
  framing, §0/§3.

## Verified — CEGAR and lazy abstraction (§4 lineage)

* **[CGJLV00]** E. Clarke, O. Grumberg, S. Jha, Y. Lu, H. Veith.
  *Counterexample-guided abstraction refinement.* CAV 2000, LNCS 1855.
  (`Clarke_Grumberg_etal_2000_CAV`)
* **[CGJLV03]** Same authors. *Counterexample-guided abstraction
  refinement for symbolic model checking.* Journal of the ACM
  50(5):752–794, September 2003. (`Clarke_Grumberg_etal_2003_JACM`)
* **[GS97]** S. Graf, H. Saïdi. *Construction of abstract state graphs
  with PVS.* CAV 1997. (`Graf_Saidi_1997_CAV`) — predicate
  abstraction, the refiner the learner replaces.
* **[HJMS02]** T. A. Henzinger, R. Jhala, R. Majumdar, G. Sutre.
  *Lazy abstraction.* POPL 2002. (`Henzinger_Jhala_etal_2002_POPL`) —
  abstract, verbatim: precision varies per region, "just enough to
  verify the desired property" — the emergent-cone comparison.

## Verified — learned assume-guarantee (§9 framing)

* **[CGP03]** J. M. Cobleigh, D. Giannakopoulou, C. S. Păsăreanu.
  *Learning assumptions for compositional verification.* TACAS 2003,
  LNCS 2619, pp. 331–346.
  (`Cobleigh_Giannakopoulou_Pasareanu_2003_TACAS`) — assumptions
  learned by L*, counterexamples from checking component and
  environment alternately: the teacher lives in the product.
* **[PGB+08]** C. S. Păsăreanu, D. Giannakopoulou, M. G. Bobaru,
  J. M. Cobleigh, H. Barringer. *Learning to divide and conquer:
  applying the L* algorithm to automate assume-guarantee reasoning.*
  Formal Methods in System Design 32:175–205, 2008.
  (`Pasareanu_Giannakopoulo_etal_2008_FMSD`) — journal treatment;
  symmetric and asymmetric rules; alphabet refinement.
* **[CAC08]** J. M. Cobleigh, G. S. Avrunin, L. A. Clarke. *Breaking
  up is hard to do: an evaluation of automated assume-guarantee
  reasoning.* ACM TOSEM 17(2), Article 7, April 2008.
  (`Cobleigh_Avrunin_Clarke_2008_TOSEM`) — the negative result §9
  answers; abstract and §6 conclusions, verified: all two-way
  decompositions, two verifiers (FLAVERS, LTSA); "for the vast
  majority of decompositions, more states were explored [with AG]
  than [monolithically]"; at the smallest sizes about half the
  subjects had no decomposition beating the monolith; the generalized
  best decomposition verified larger sizes on only 8/32 subjects
  (FLAVERS) and 0/30 (LTSA). NB the bottleneck named by the data is
  premise size (each side of a two-way split is half the system) and
  decomposition choice — not L* overhead; do not paraphrase it as
  "learning overhead".
* **[AMN05]** R. Alur, P. Madhusudan, W. Nam. *Symbolic compositional
  verification by learning assumptions.* CAV 2005, LNCS 3576,
  pp. 548–562. (`Alur_Madhusuda_Nam_2005_CAV`) — the assumption goes
  symbolic (BDDs, NuSMV); the teacher does not move.
* **[CFC+09]** Y.-F. Chen, A. Farzan, E. M. Clarke, Y.-K. Tsay,
  B.-Y. Wang. *Learning minimal separating DFA's for compositional
  verification.* TACAS 2009. (`Chen_Farzan_etal_2009_TACAS`)
* **[FCC+08]** A. Farzan, Y.-F. Chen, E. M. Clarke, Y.-K. Tsay,
  B.-Y. Wang. *Extending automated compositional verification to the
  full class of ω-regular languages.* TACAS 2008.
  (BtL `Farzan_Chen_Clarke_etal_2008_TACAS`)

## Verified — symmetry (Remark 5.4)

* **[ID96]** C. N. Ip, D. L. Dill. *Better verification through
  symmetry.* Formal Methods in System Design 9:41–75, 1996.
  (BtL `Ip_Dill_1996_FMSD`)
* **[CEFJ96]** E. M. Clarke, R. Enders, T. Filkorn, S. Jha.
  *Exploiting symmetry in temporal logic model checking.* Formal
  Methods in System Design 9:77–104, 1996.
  (BtL `Clarke_Enders_Filkorn_Jha_1996_FMSD`)
* **[ES96]** E. A. Emerson, A. P. Sistla. *Symmetry and model
  checking.* Formal Methods in System Design 9, 1996.
  (BtL `Emerson_Sistla_1996_FMSD`)

## Verified — well-structured leaves (§7)

* **[AČJT96]** P. A. Abdulla, K. Čerāns, B. Jonsson, Y.-K. Tsay.
  *General decidability theorems for infinite-state systems.* LICS
  1996. (`Abdulla_Cerans_etal_1996_LICS`) — abstract states
  decidability of safety properties given as prefix-closed trace sets
  of a finite automaton: our certification obligation verbatim.
* **[AČJT00]** Same authors. *Algorithmic analysis of programs with
  well quasi-ordered domains.* Information and Computation
  160:109–127, 2000. (`Abdulla_Cerans_etal_2000_IC`)
* **[FS01]** A. Finkel, Ph. Schnoebelen. *Well-structured transition
  systems everywhere!* Theoretical Computer Science, 2001.
  (`Finkel_Schnoebelen_2001_TCS`)

## Verified — the ω line (§2.6, §10.3)

* **[Arn85]** A. Arnold. *A syntactic congruence for rational
  ω-languages.* Theoretical Computer Science 39:333–335, 1985.
  (BtL `Arnold_1985_TCS`)
* **[MS97]** O. Maler, L. Staiger. *On syntactic congruences for
  ω-languages.* TCS, 1997. (BtL `Maler_Staiger_1997_TCS`)
* **[CNP93]** H. Calbrix, M. Nivat, A. Podelski. *Ultimately periodic
  words of rational ω-languages.* MFPS 1993.
  (BtL `Calbrix_Nivat_Podelski_1993_MFPS`)
* **[MP95]** O. Maler, A. Pnueli. *On the learnability of infinitary
  regular sets.* Information and Computation, 1995.
  (BtL `Maler_Pnueli_1995_IC`)
* **[AF16]** D. Angluin, D. Fisman. *Learning regular omega
  languages.* Theoretical Computer Science 650:57–72, 2016.
  (BtL `Angluin_Fisman_2016_TCS`)
* **[LCZL17]** Y. Li, Y.-F. Chen, L. Zhang, D. Liu. *A novel learning
  algorithm for Büchi automata based on family of DFAs and
  classification trees.* TACAS 2017. (BtL `Chen_Liu_2017_TACAS`)
* **[BL21]** L. Bohn, C. Löding. *Constructing deterministic
  ω-automata from examples by an extension of the RPNI algorithm.*
  MFCS 2021. (BtL `Bohn_Loding_2021_MFCS`)

## Verified — symbolic walks (§10.4) and house baseline (§9)

* **[Bry86]** R. E. Bryant. *Graph-based algorithms for Boolean
  function manipulation.* IEEE Transactions on Computers, 1986.
  (BtL `Bryant_1986_TC`)
* **[BCM+92]** J. R. Burch, E. M. Clarke, K. L. McMillan, D. L. Dill,
  L. J. Hwang. *Symbolic model checking: 10^20 states and beyond.*
  Information and Computation, 1992. (BtL `Burch_Clarke_etal_1992_IC`)
* **[CLS01]** G. Ciardo, G. Lüttgen, R. Siminiceanu. *Saturation: an
  efficient iteration strategy for symbolic state-space generation.*
  TACAS 2001. (BtL `Ciardo_Luttgen_Siminiceanu_2001_TACAS`; archive
  copy is the ICASE report version.)
* **[TPHK09]** Y. Thierry-Mieg, D. Poitrenaud, A. Hamez, F. Kordon.
  *Hierarchical set decision diagrams and regular models.* TACAS
  2009. (BtL `ThierryMieg_etal_2009-TACAS`)
* **[TM21]** Y. Thierry-Mieg. *Symbolic and structural
  model-checking.* Fundamenta Informaticae 183(3–4):319–343, 2021.
  (`thierrymieg2021_symbolic_structural_mc`)

## Verified — comparator abstraction disciplines (§9)

* **[BLD18]** B. Berthomieu, D. Le Botlan, S. Dal Zilio. *Petri net
  reductions for counting markings.* 2018; archive copy is
  arXiv:1807.02973. (`berthomieu2018_reductions_counting_markings`)
* **[ABD22]** N. Amat, B. Berthomieu, S. Dal Zilio. *A polyhedral
  abstraction for Petri nets and its application to SMT-based model
  checking.* 2022; archive copy is the HAL version (hal-03455697).
  (`amat2022_polyhedral_abstraction_smt`)
* **[AC22]** N. Amat, L. Chauvet. *Kong: a tool to squash concurrent
  places.* Petri Nets 2022. (`amat2022_kong_concurrent_places`)
* **[Laa18]** A. Laarman. *Stubborn transaction reduction.* 2018;
  archive copy is the with-proofs version.
  (`laarman2018_stubborn_transaction_reduction`) — optional POR
  comparator.

## Still to acquire (do not cite)

* AG origins one-liner: Misra & Chandy 1981; Jones 1983; Pnueli 1985;
  Grumberg & Long 1994 — or compress to one textbook pointer. The coda
  currently names assume-guarantee without an origins cite; acceptable
  for a working draft, must be settled before submission.
* Pelánek 2007 — BEEM benchmark suite (SPIN workshop); needed when §9
  runs on BEEM/DVE.
* Kordon et al. — MCC reports (corpus + NUPN format); needed when §9
  runs on MCC.
* Holzmann (SPIN), Kant et al. 2015 (LTSmin) — only if external
  explicit checkers are ever named as more than reference points.
* Kearns & Vazirani 1994; Isberner–Howar–Steffen TTT (RV 2014) /
  LearnLib (CAV 2015) — only if the implementation section discusses
  learner engineering.
* Exact venue lines for [BLD18], [ABD22], [Laa18], [AN80]: the archive
  copies are preprint/report versions whose published venue is not
  stated in the text we hold; confirm before camera-ready rather than
  cite from memory.
