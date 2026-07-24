; A rotating ring: cell i starts at i (an indexed init — the value uses the
; binder), and one event rotates the whole array simultaneously: the binder
; inside a single (do …) clause makes one synchronous multi-assign, a true
; permutation, not a cascade. Reachable states: the N cyclic shifts.
(param N 6)
(array a N 0 N)
(shape (spine (forall (i N) (at a i))))
(init (forall (i N) (do (:= (at a i) i))))
(event rot
  (do (forall (i N) (:= (at a i) (at a (% (+ i 1) N))))))
(reach R saturate)
(count R)
(expect R 6)
