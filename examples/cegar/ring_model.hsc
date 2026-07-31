; Token ring of N identical stations — the model alone, no commands:
; drivers pull it in with (input ring_model.hsc) and run what they
; want on it (symbolic, explicit, cegar) without touching this file.
; Station: 0 idle, 1 holds the token, 2 critical. The monitor leaf
; counts stations in the critical section.
(param N 5)
(array st N 0 3)
(leaf mon 0 3)
(shape (spine (forall (i N) (at st i)) mon))
(init (st_0 1))
(event enter (exists (i N)
  (when (== (at st i) 1) (< mon 2))
  (do (:= (at st i) 2) (+= mon 1))))
(event exit (exists (i N)
  (when (== (at st i) 2) (>= mon 1))
  (do (:= (at st i) 1) (-= mon 1))))
(event pass (exists (i N)
  (when (== (at st i) 1) (== (at st (% (+ i 1) N)) 0))
  (do (:= (at st i) 0) (:= (at st (% (+ i 1) N)) 1))))
