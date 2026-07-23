; Towers of Hanoi, parametric: N rings, P poles. One file, one template.
; Compare tools/hanoi.py (the external generator this subsumes) and
; ~/git/ITS-Exercise/hanoi.gal (GAL: transition parameters + for/if/abort
; where a quantified guard suffices, and a goal property spelled cell by
; cell where a forall suffices).
(param N 7)
(param P 3)
(array r N 0 P)
; smallest ring r_0 laid deepest: an event's support is a suffix
(shape (spine (forall (i N) (at r (- (- N 1) i)))))
(init)
; move ring k from pole a to pole b: no smaller ring on a or b
(event move
  (exists (k N) (exists (a P) (exists (b P)
    (when (!= a b) (== (at r k) a)
          (forall (j k) (and (!= (at r j) a) (!= (at r j) b))))
    (do (:= (at r k) b))))))
(reach R saturate)
; with 3 poles every configuration is reachable: P^N states
(count R)
(expect R 2187)
; the goal: every ring on the last pole — one quantified atom, one state
(select goal R (forall (i N) (== (at r i) (- P 1))))
(expect goal 1)
