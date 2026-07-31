; The bugged clients variant, model only: grants no longer check that
; the server is free, so a second grant lands on an outstanding one.
(param K 3)
(leaf srv 0 (+ K 1))
(array cl K 0 2)
(leaf mon 0 3)
(shape (spine srv (forall (i K) (at cl i)) mon))
(init)
(event g (exists (i K)
  (when (== (at cl i) 0) (< mon 2))
  (do (:= srv (+ i 1)) (:= (at cl i) 1) (+= mon 1))))
(event r (exists (i K)
  (when (== srv (+ i 1)) (== (at cl i) 1) (== mon 1))
  (do (:= srv 0) (:= (at cl i) 0) (-= mon 1))))
