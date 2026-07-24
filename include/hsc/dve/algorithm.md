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
  `event`: `when` = `(== P_state #s)` plus the compiled guard, then one
  `(do …)` clause per step — the control move first, then each effect in
  list order.
* **Effects compose.** DVE executes an effect list left to right; the
  surface expresses exactly that as repeated `(do …)` clauses, compiled to
  `op_table::compose` — atomic sequential steps, arrays and all. No forward
  substitution into a simultaneous assign: a sync assign entangles supports
  and hurts saturation, so it is reserved for semantics that truly need one
  (a swap, SMV) — DVE never does.
* **Guards.** `Proc.state` atoms become `(== Proc_state #s)`. A bare
  arithmetic term in boolean position is `(!= e 0)`; `=>`/`imply` expands to
  `(or (not a) b)`. Guards stay expression trees in the emitted forms —
  disjunction, arithmetic, crossing atoms are printed as written and it is
  the *surface's* business to accept, case, or refuse them.
* **Channels** (rendezvous, `system async`): every (send, recv) transition
  pair on a channel fuses into one event — both state atoms and guards
  conjoined, then the composed steps in reference order: sender effects,
  sender move, `recvvar := sendexpr` (the sent value reads the sender's
  scope, the variable resolves in the receiver's), receiver effects,
  receiver move. Same-process pairs are skipped. `system sync` (lockstep)
  is refused.
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
