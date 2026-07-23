; Dining philosophers, parametric. st_i: 0 thinking, 1 holds its fork,
; 2 eating; f_i: fork i taken. Philosopher i uses forks i and (i+1)%N —
; index arithmetic in the template, grounded at expansion.
(param N 5)
(array st N 0 3)
(array f N 0 2)
; component-interleaved layout: philosopher i's state beside its fork
(shape (spine (forall (i N) (at st i) (at f i))))
(init)
(event take1
  (exists (i N)
    (when (== (at st i) 0) (== (at f i) 0))
    (do (:= (at f i) 1) (:= (at st i) 1))))
(event take2
  (exists (i N)
    (when (== (at st i) 1) (== (at f (% (+ i 1) N)) 0))
    (do (:= (at f (% (+ i 1) N)) 1) (:= (at st i) 2))))
(event release
  (exists (i N)
    (when (== (at st i) 2))
    (do (:= (at st i) 0) (:= (at f i) 0) (:= (at f (% (+ i 1) N)) 0))))
(reach R saturate)
(count R)
; the saturation schedule against the naive iteration: one answer
(reach RN naive)
(expect RN 82)
(expect R 82)
; no two neighbours eat at once: the shared fork forbids it. A quantified
; crossing predicate is a filter term; apply takes its image of R.
(apply clash
  (when (exists (i N) (and (== (at st i) 2) (== (at st (% (+ i 1) N)) 2))))
  R)
(expect clash 0)
