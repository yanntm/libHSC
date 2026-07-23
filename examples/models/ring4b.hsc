; ring4b: the ring4 net under a balanced shape — pair(pair(a,b), pair(c,d)).
; Same net, same 20 states, same stars-and-bars oracle; but every crossing
; query now splits a subdiagram, not a leaf: a and b sit inside the head
; pair, so (< a c) is the deep-head case. Counts must match ring4 exactly:
; expressions are written on flat names, the shape is not their business.
(leaf a 0 4)
(leaf b 0 4)
(leaf c 0 4)
(leaf d 0 4)
(shape (balanced a b c d))
(init (a 3))
(event ab (when (> a 0)) (do (-= a 1) (+= b 1)))
(event bc (when (> b 0)) (do (-= b 1) (+= c 1)))
(event cd (when (> c 0)) (do (-= c 1) (+= d 1)))
(event da (when (> d 0)) (do (-= d 1) (+= a 1)))
(reach R saturate)
(expect R 20)
; deep head: a is a level below the top cut's head pair.
(select Slt R (< a c))
(expect Slt 7)
(select Sad R (< a d))
(expect Sad 7)
(select Sbc R (< b c))
(expect Sbc 7)
(select Seq R (== a c))
(expect Seq 6)
(select Sne R (!= a d))
(expect Sne 14)
(select Sle R (<= b d))
(expect Sle 13)
; within the head pair: both sides above the top cut, resolved a level up.
(select Sab R (< a b))
(expect Sab 7)
; conjunction across both cuts.
(select Sconj R (< a c) (== b 0))
(expect Sconj 4)
