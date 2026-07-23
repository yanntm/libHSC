; Layout experiment, variant 3/3: blocks of G, philosopher i's state beside
; its fork. Model identical across variants; only the shape differs — the
; count must agree (a cross-layout differential), nodes and bill may not.
(param N 256)
(array st N 0 3)
(array f N 0 2)
(param G 16)
(param B (/ N G))
(shape (spine (forall (b B) (spine (forall (k G) (at st (+ (* b G) k)) (at f (+ (* b G) k)))))))
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
