; unbounded: every leaf is a plain Int — no declared domain anywhere.
; What bounds each orbit is the event's own guard, as in any honest model;
; guards are symbolic (evaluated on values present, no table materialized).
; Oracle by hand: c climbs 0..5 (guard c<5 before +=1), x steps 0,3,6,9
; (guard x<7 before +=3), d descends 0..-3 (guard d>-3 before -=1).
; Reach = 6 * 4 * 4 = 96 independent combinations.
(leaf c)
(leaf x)
(leaf d)
(shape (spine c x d))
(init)
(event ic (when (< c 5)) (do (+= c 1)))
(event ix (when (< x 7)) (do (+= x 3)))
(event dd (when (> d -3)) (do (-= d 1)))
(reach R saturate)
(expect R 96)
(reach Rn naive)
(expect Rn 96)
; c < x across the first cut: per x in {0,3,6,9} the c's below it
; (0 + 3 + 6 + 6) = 15, times 4 values of d.
(select Scx R (< c x))
(expect Scx 60)
; c == x only at 0 and 3: 2 * 4.
(select Seq R (== c x))
(expect Seq 8)
; d < c with d nonpositive: d in {-3,-2,-1} always (18), d=0 needs c>0 (5);
; 23 pairs, times 4 values of x.
(select Sdc R (< d c))
(expect Sdc 92)
; separable atoms on an unbounded leaf, negatives included.
(select Snn R (>= d 0))
(expect Snn 24)
(select S0 R (== c 0))
(expect S0 16)
; conjunction: crossing then separable.
(select Sconj R (< c x) (== d -2))
(expect Sconj 15)
