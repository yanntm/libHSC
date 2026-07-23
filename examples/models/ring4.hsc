; ring4: 4 places in a ring, 3 indistinct tokens circulating forward.
; Multi-token on purpose: the cross-level queries below must case on the
; token count (split_equiv), not lean on a one-safe encoding.
; Oracle: distributions of 3 tokens over 4 places = C(6,3) = 20 states;
; per-query counts by stars and bars, stated at each expect.
(leaf a 0 4)
(leaf b 0 4)
(leaf c 0 4)
(leaf d 0 4)
(shape (spine a b c d))
(init (a 3))
(event ab (when (> a 0)) (do (-= a 1) (+= b 1)))
(event bc (when (> b 0)) (do (-= b 1) (+= c 1)))
(event cd (when (> c 0)) (do (-= c 1) (+= d 1)))
(event da (when (> d 0)) (do (-= d 1) (+= a 1)))
(reach R saturate)
(expect R 20)
; a < c crosses the top cut: a is the head coordinate, c sits two levels
; down the tail. 7 = over (a,c) in {(0,1),(0,2),(0,3),(1,2)}, the b+d
; completions 3+2+1+1.
(select Slt R (< a c))
(expect Slt 7)
(select Sgt R (> a c))
(expect Sgt 7)
; a == c: (0,0) leaves b+d=3 (4 ways), (1,1) leaves b+d=1 (2 ways).
(select Seq R (== a c))
(expect Seq 6)
(select Sne R (!= a c))
(expect Sne 14)
(select Sle R (<= a c))
(expect Sle 13)
(select Sge R (>= a c))
(expect Sge 13)
; b < d resolves a level deeper: both coordinates below the first cut.
(select Sbd R (< b d))
(expect Sbd 7)
; separable atom: a frozen at 0 leaves b+c+d=3, C(5,2)=10 ways.
(select S0 R (== a 0))
(expect S0 10)
; conjunction, crossing then separable: a<c with b=0 is a+c+d=3, 4 ways.
(select Sconj R (< a c) (== b 0))
(expect Sconj 4)
; reflexivity needs no split: a < a is empty, a <= a is everything.
(select Sirr R (< a a))
(expect Sirr 0)
(select Srefl R (<= a a))
(expect Srefl 20)
