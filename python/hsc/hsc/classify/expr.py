"""Classifiers: a small interned expression language over leaf coordinates.

Expressions are the terms that travel during a partition traversal. Two
properties matter and both are structural:

- **interned**: structurally equal expressions are one object, so an
  expression *is* its canonical code and equality is a pointer test. This
  is what makes deduplication of subqueries identical to cache sharing.
- **normalising on construction**: substituting a value rebuilds bottom-up
  and folds what it can, so a residual that has lost its dependence on a
  coordinate says so in its code rather than merely in its meaning.

Normalisation strength is a cost parameter, never a soundness one: an
equality this module fails to detect costs a redundant subquery and nothing
else. `plus` and the propositional builders fold constants and flatten
because that is where the interesting residual collapses happen; anything
stronger belongs to a leaf's own rewriter.
"""

from __future__ import annotations

from typing import Any, Callable, Dict, FrozenSet, List, Optional, Tuple

_TABLE: Dict[Any, "Expr"] = {}
_NEXT: List[int] = [1]


class Expr:
    """Base class. Instances are interned; use the builders below."""

    __slots__ = ("uid", "vars")

    uid: int
    vars: FrozenSet[str]

    def _init(self, variables: FrozenSet[str]) -> None:
        self.vars = variables
        self.uid = _NEXT[0]
        _NEXT[0] += 1

    def subst(self, name: str, value: Any) -> "Expr":
        raise NotImplementedError

    def is_ground(self) -> bool:
        return not self.vars


class Const(Expr):
    __slots__ = ("value",)

    def __init__(self, value: Any) -> None:
        self.value = value
        self._init(frozenset())

    def subst(self, name: str, value: Any) -> Expr:
        return self

    def __repr__(self) -> str:
        return repr(self.value)


class Var(Expr):
    __slots__ = ("name",)

    def __init__(self, name: str) -> None:
        self.name = name
        self._init(frozenset((name,)))

    def subst(self, name: str, value: Any) -> Expr:
        return const(value) if name == self.name else self

    def __repr__(self) -> str:
        return self.name


class Op(Expr):
    """An applied leaf-registered function. Folds when every argument is ground.

    An `Op` carries the *builder* that made it, not just its symbol, and
    substitution rebuilds through that builder. This is the difference
    between normalising an expression when a client writes it and
    normalising it while it travels -- and only the second one matters,
    since currying is where residuals get their chance to coincide."""

    __slots__ = ("opname", "fn", "args", "rebuild")

    def __init__(
        self,
        opname: str,
        fn: Callable[..., Any],
        args: Tuple[Expr, ...],
        rebuild: Callable[..., Expr],
    ) -> None:
        self.opname = opname
        self.fn = fn
        self.args = args
        self.rebuild = rebuild
        self._init(frozenset().union(*[a.vars for a in args]) if args else frozenset())

    def subst(self, name: str, value: Any) -> Expr:
        if name not in self.vars:
            return self
        return self.rebuild(*[a.subst(name, value) for a in self.args])

    def __repr__(self) -> str:
        return f"{self.opname}({','.join(map(repr, self.args))})"


def _intern(key: Any, build: Callable[[], Expr]) -> Expr:
    got = _TABLE.get(key)
    if got is None:
        got = _TABLE[key] = build()
    return got


def const(value: Any) -> Expr:
    return _intern(("c", type(value).__name__, value), lambda: Const(value))


def var(name: str) -> Expr:
    return _intern(("v", name), lambda: Var(name))


def op(
    opname: str,
    fn: Callable[..., Any],
    *args: Expr,
    rebuild: Optional[Callable[..., Expr]] = None,
) -> Expr:
    if all(a.is_ground() for a in args):
        return const(fn(*[a.value for a in args]))  # type: ignore[attr-defined]
    build = rebuild if rebuild is not None else (lambda *a: op(opname, fn, *a))
    return _intern(
        ("o", opname, tuple(a.uid for a in args)),
        lambda: Op(opname, fn, tuple(args), build),
    )


# ---- builders that normalise harder than plain application ------------


def _flat(opname: str, args: Tuple[Expr, ...]) -> List[Expr]:
    out: List[Expr] = []
    for a in args:
        if isinstance(a, Op) and a.opname == opname:
            out.extend(a.args)
        else:
            out.append(a)
    return out


def plus(*args: Expr) -> Expr:
    """Flattening, constant-folding sum. Residuals that differ only by an
    already-accumulated constant get distinct codes; residuals that agree
    get the same one, which is the collapse the traversal lives on."""
    terms = _flat("plus", args)
    total = 0
    rest: List[Expr] = []
    for t in terms:
        if isinstance(t, Const):
            total += t.value
        else:
            rest.append(t)
    if not rest:
        return const(total)
    rest.sort(key=lambda e: e.uid)
    if total:
        rest = [const(total)] + rest
    if len(rest) == 1:
        return rest[0]
    return _intern(
        ("o", "plus", tuple(e.uid for e in rest)),
        lambda: Op(
            "plus",
            lambda *vs: sum(vs),
            tuple(rest),
            plus,
        ),
    )


def land(*args: Expr) -> Expr:
    terms = _flat("and", args)
    rest: List[Expr] = []
    for t in terms:
        if isinstance(t, Const):
            if not t.value:
                return const(False)
            continue
        if t not in rest:
            rest.append(t)
    if not rest:
        return const(True)
    rest.sort(key=lambda e: e.uid)
    if len(rest) == 1:
        return rest[0]
    return _intern(
        ("o", "and", tuple(e.uid for e in rest)),
        lambda: Op(
            "and",
            lambda *vs: all(vs),
            tuple(rest),
            land,
        ),
    )


def lor(*args: Expr) -> Expr:
    terms = _flat("or", args)
    rest: List[Expr] = []
    for t in terms:
        if isinstance(t, Const):
            if t.value:
                return const(True)
            continue
        if t not in rest:
            rest.append(t)
    if not rest:
        return const(False)
    rest.sort(key=lambda e: e.uid)
    if len(rest) == 1:
        return rest[0]
    return _intern(
        ("o", "or", tuple(e.uid for e in rest)),
        lambda: Op(
            "or",
            lambda *vs: any(vs),
            tuple(rest),
            lor,
        ),
    )


def lnot(e: Expr) -> Expr:
    return op("not", lambda v: not v, e)


def eq(a: Expr, b: Expr) -> Expr:
    return op("eq", lambda x, y: x == y, a, b)


def lt(a: Expr, b: Expr) -> Expr:
    return op("lt", lambda x, y: x < y, a, b)


def ind(e: Expr) -> Expr:
    """Indicator: a predicate read as 0/1, so predicates can be summed."""
    return op("ind", lambda v: 1 if v else 0, e)


def mod(a: Expr, k: int) -> Expr:
    """Residue, with the one rewrite that matters: an accumulated constant
    inside a sum is reduced modulo k before the code is formed, so that
    `(3+c) mod 3` and `c mod 3` are one code rather than two.

    This is a leaf's arithmetic rewriter in miniature, and it is a *cost*
    rule only: a residue it failed to detect would cost redundant subqueries
    that the kernel merge recovers on the way back up, never a wrong
    answer."""
    if isinstance(a, Op) and a.opname == "plus":
        args = list(a.args)
        if args and isinstance(args[0], Const):
            a = plus(const(args[0].value % k), *args[1:])
    return op(f"mod{k}", lambda v: v % k, a, rebuild=lambda x: mod(x, k))


TRUE = const(True)
FALSE = const(False)
