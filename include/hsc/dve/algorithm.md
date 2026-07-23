# dve — grammar, model, and the mapping to the surface

## 1. T2M: the language (after `Divine.xtext`)

```
spec       ::= (vardecl | constdecl | chandecl)* process* 'system' ('async'|'sync') ';'
constdecl  ::= 'const' ('int'|'byte') NAME '=' expr (',' NAME '=' expr)* ';'
vardecl    ::= ('int'|'byte') one (',' one)* ';'
one        ::= NAME ('=' expr)? | NAME '[' INT ']' ('=' '{' expr (',' expr)* '}')?
chandecl   ::= 'channel' NAME (',' NAME)* ';'
process    ::= 'process' NAME '{' (vardecl|constdecl)*
               ('state' NAME (',' NAME)* ';' 'init' NAME ';'
                ('accept' NAME+ ';')? ('commit' NAME+ ';')?)?
               ('assert' NAME ':' expr (',' NAME ':' expr)* ';')?
               'trans' trans (',' trans)* ';' '}'
trans      ::= NAME '->' NAME '{' ('guard' expr ';')? (sync ';')? ('effect' assign (',' assign)* ';')? '}'
sync       ::= 'sync' NAME '!' expr? | 'sync' NAME '?' varaccess?
assign     ::= varaccess '=' expr
varaccess  ::= NAME | NAME '[' expr ']'
```

Expression precedence, loosest first (DiVinE's own order):
`=> || && imply and or` · `| & ^` · `== != < <= > >=` · `<< >>` · `+ -` ·
`* / %` · prefix `! not - ~` · atoms (INT, `true`/`false`, NAME,
`NAME[expr]`, `Proc.state`, parentheses). Comments `//` and `/* */`.

The AST keeps names unresolved and expressions as trees; nothing is typed or
numbered at this layer. One deviation from the Xtext: STRING literals are
refused (the corpus never uses them meaningfully).

## 2. M2M: model → surface forms (after `DveToGALTransformer`)

* **Names.** Global `x` → leaf `glob_x`; process-local `x` of `P` → `P_x`;
  the control state of `P` → leaf `P_state`, valued by the *declaration
  index* of its states, `init` giving the initial value. `const` names fold
  away at transform time (they are compile-time ints, evaluated with the
  constants seen so far).
* **Types.** `int` and `byte` both map to Int leaves — bounds are not
  required by the surface. `byte` declares the opt-in `[0,256)` bound as
  model information. DiVinE's byte *wraparound* (mod-256 arithmetic) is not
  yet reproduced: arithmetic is emitted plain, and a model whose state count
  relies on wrapping will disagree loudly (overflow or count mismatch) —
  recorded per model by the harness, resolved when the theory grows a
  wrapping shift.
* **A local transition** `s -> d {guard g; effect a…}` of `P` becomes one
  `event`: `when` = `(== P_state #s)` plus the compiled guard, `do` =
  `(:= P_state #d)` plus the compiled effects.
* **Effects parallelize.** DVE executes an effect list left to right; surface
  actions are simultaneous, one per leaf. Walking the list with forward
  substitution (each right-hand side rewritten by the scalar assignments
  already seen) turns the sequence into equivalent parallel assigns. A read
  of an array cell after a write to the *same array through a variable
  index* cannot be rewritten soundly — refused loudly, none in the corpus so
  far.
* **Guards.** `Proc.state` atoms become `(== Proc_state #s)`. A bare
  arithmetic term in boolean position is `(!= e 0)`; `=>`/`imply` expands to
  `(or (not a) b)`. Guards stay expression trees in the emitted forms —
  disjunction, arithmetic, crossing atoms are printed as written and it is
  the *surface's* business to accept (§6), case (§7), or refuse them.
* **Channels** (rendezvous, `system async`): every (send, recv) transition
  pair on a channel fuses into one event — both state atoms and guards
  conjoined, sender effects, sender move, `recvvar := sendexpr` (value
  passing), receiver effects, receiver move, parallelized as above. A pair
  from one process is killed by its own contradictory state atoms. `system
  sync` (lockstep) is refused.
* **Transient states.** A state named `trans_*` is DiVinE's transient
  convention (its occupancy is not a real state). Recorded on the model;
  reproducing the semantics is deferred, and the harness marks affected
  models.
* **`accept` / `commit` / `assert`** are parsed, kept on the model, and not
  emitted: they belong to property layers we do not target yet.

## 3. What the emitted file looks like

Declarations first (`leaf`, with `[0,256)` for bytes), one `shape` (a spine
in declaration order — deliberately naive: deriving hierarchy from the
process structure is exactly the encoding experiment the DVE model is kept
around for), `init`, the events, then `(reach R saturate)` and `(count R)`.
The `.prop1.reach` criteria of BEEM map to `select` once their operators are
in scope.
