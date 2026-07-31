; K clients and a server — the cegar paper's worked example (§6),
; model only: drivers pull it in with (input clients_model.hsc).
; A grant sets the server busy and the client busy; the monitor leaf
; counts outstanding grants ((== mon 2) = a grant on top of a grant).
(param K 4)
(leaf srv 0 (+ K 1))   ; 0 free, i+1 busy with client i
(array cl K 0 2)       ; 0 idle, 1 granted
(leaf mon 0 3)
(shape (spine srv (forall (i K) (at cl i)) mon))
(init)
(event g (exists (i K)
  (when (== srv 0) (== (at cl i) 0) (< mon 2))
  (do (:= srv (+ i 1)) (:= (at cl i) 1) (+= mon 1))))
(event r (exists (i K)
  (when (== srv (+ i 1)) (== (at cl i) 1) (== mon 1))
  (do (:= srv 0) (:= (at cl i) 0) (-= mon 1))))
