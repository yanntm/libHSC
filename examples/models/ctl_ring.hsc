; ctl_ring: the ring4 model (3 tokens circulating over 4 places, 20 states,
; strongly connected, no deadlock) with CTL formulas hand-checked below.
; Forward-form fragment only: formulas whose conversion leaves a universal
; operator under the seed are UNKNOWN until the inverse lands.
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
(alt ALL ab bc cd da)
(reach R saturate)
(expect R 20)
; every state lies on a cycle: the deflationary closure keeps all of R
(gfp G ALL R)
(expect G 20)
; all three tokens can gather on d
(ctl P1 (EF (== d 3)))
(expect-ctl P1 TRUE)
; the bound holds everywhere: AG is asked as the emptiness of EF (> a 3)
(ctl P2 (AG (<= a 3)))
(expect-ctl P2 TRUE)
; a can stay positive forever: ab bc cd da from a=3 keeps a >= 2
(ctl P3 (EG (> a 0)))
(expect-ctl P3 TRUE)
; one step: ab puts a token on b
(ctl P4 (EX (== b 1)))
(expect-ctl P4 TRUE)
; AF (a == 0) is the negation of P3
(ctl P5 (AF (== a 0)))
(expect-ctl P5 FALSE)
; ab ab bc bc reaches c=2 with a > 0 all along (3,2,1,1,1)
(ctl P6 (EU (> a 0) (== c 2)))
(expect-ctl P6 TRUE)
; tokens are conserved: no dead marking
(ctl P7 (EF (deadlock)))
(expect-ctl P7 FALSE)
; AX of a tautology: EX (< b 0) from the seed is empty
(ctl P8 (AX (>= b 0)))
(expect-ctl P8 TRUE)
; a universal operator under EF needs the preimage: refused for now
(ctl P9 (EF (AG (> a 0))))
(expect-ctl P9 UNKNOWN)
; E[a>0 W false] = EG (a > 0)
(ctl P10 (EW (> a 0) false))
(expect-ctl P10 TRUE)
