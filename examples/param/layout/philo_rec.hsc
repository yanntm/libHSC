; Layout experiment, variant 4: DEPTHREC grain — every level of the shape
; is a right-comb of G sub-blocks, recursively, flat below G philosophers
; (libITS DepthRecStrategy). rec 2 is the component-grouped balanced tree.
; Model identical across variants; only the shape differs — the count must
; agree (a cross-layout differential), nodes and bill may not.
(param N 256)
(array st N 0 3)
(array f N 0 2)
(param G 3)
(shape (blocked rec G (i N) (at st i) (at f i)))
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
