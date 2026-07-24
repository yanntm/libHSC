; Layout experiment, variant 1/3: flat spine, philosopher i's state beside
; its fork. Model identical across variants; only the shape differs — the
; count must agree (a cross-layout differential), nodes and bill may not.
(param N 256)
(array st N 0 3)
(array f N 0 2)
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
(nodes R)
(bill)
