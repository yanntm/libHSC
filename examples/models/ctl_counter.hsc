; ctl_counter: a counter 0..3 that only goes up — acyclic, one deadlock at
; x = 3. The deadlock-terminated path semantics of the contest, hand-checked.
(leaf x 0 4)
(shape (spine x))
(event inc (when (< x 3)) (do (+= x 1)))
(reach R saturate)
(expect R 4)
; acyclic: the deflationary closure of the step empties R
(gfp G inc R)
(expect G 0)
; the deadlock is reachable, and unavoidable
(ctl Q1 (EF (deadlock)))
(expect-ctl Q1 TRUE)
(ctl Q2 (AF (deadlock)))
(expect-ctl Q2 TRUE)
; EG holds at a deadlocked state satisfying the operand ...
(ctl Q3 (EG (>= x 0)))
(expect-ctl Q3 TRUE)
; ... but not when every maximal path leaves the operand before stopping
(ctl Q4 (EG (< x 3)))
(expect-ctl Q4 FALSE)
; EX is false at the deadlock and AX true there
(ctl Q5 (EF (and (== x 3) (EX true))))
(expect-ctl Q5 FALSE)
(ctl Q6 (EF (and (== x 3) (AX false))))
(expect-ctl Q6 TRUE)
; AG (EF x==3): the deadlock is reached from everywhere
(ctl Q9 (AG (EF (== x 3))))
(expect-ctl Q9 TRUE)
; AF (AG x==3): every path ends stuck at 3
(ctl Q10 (AF (AG (== x 3))))
(expect-ctl Q10 TRUE)
; the inverse of inc: x -= 1 on {1,2,3}; x=3 has one predecessor, and the
; backward closure from it is all of R
(invert DEC inc R)
(select X3 R (== x 3))
(apply P3 DEC X3)
(expect P3 1)
(reach RB DEC from X3)
(expect RB 4)
; E[x<2 U x==2]: 0,1,2
(ctl Q7 (EU (< x 2) (== x 2)))
(expect-ctl Q7 TRUE)
(ctl Q8 (EU (< x 1) (== x 2)))
(expect-ctl Q8 FALSE)
