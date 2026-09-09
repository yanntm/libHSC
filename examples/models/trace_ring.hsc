; trace_ring: paths on the ring4 model (3 tokens over 4 places, 20 states).
; A path is a shortest run from a set to a set, found symbolically and
; printed as word literals and event names; its length is hand-checked.
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
(word I (a 3))
; all three tokens on d: 3 moves of each of ab, bc, cd = 9 steps
(select D3 R (== d 3))
(path P1 I D3)
(expect-path P1 9)
; one token on d: ab bc cd = 3 steps
(select D1 R (== d 1))
(path P2 I D1)
(expect-path P2 3)
; through states keeping a > 0: the last token may not leave a, so d == 3
; is unreachable under that constraint
(path P3 I D3 through (> a 0))
(expect-path P3 none)
; but d == 1 is: 3 steps with a >= 2 on the way
(path P4 I D1 through (> a 0))
(expect-path P4 3)
; the source and target meet: a zero-length path
(path P5 R D3)
(expect-path P5 0)
; witness trees of ctl verdicts: EF (d == 3) is a 9-step path; AG (a <= 3)
; holds by exhaustion (no path); EF (EG (a > 0)) a path then a lasso
(ctl W1 (EF (== d 3)))
(witness W1)
(expect-path W1 9)
(ctl W2 (AG (<= a 3)))
(witness W2)
(expect-path W2 0)
(ctl W3 (EF (AG (> a 0))))
(witness W3)
; a witness, not the shortest overall: the end state is chosen first, then
; its predecessor, then a shortest run from the initial state to it
(ctl W4 (EF (and (== d 1) (EX (== d 2)))))
(witness W4)
(expect-path W4 8)
; EG through a lasso: three tokens circulating with a > 0 kept
(ctl W5 (EG (> a 0)))
(witness W5)
