; The event algebra: alt (sum — nondeterministic choice), seq (compose),
; reach over an explicit event term, the no-argument default as the big
; ALT, and the zero term of a dead event (absent from an alt, fatal to a
; seq). Oracles are exact by hand.
(leaf x 0 10)
(leaf y 0 10)
(shape (spine x y))
(init)
(event ix (when (< x 2)) (do (+= x 1)))
(event iy (when (< y 2)) (do (+= y 1)))

; the default system is the ALT of every event: x,y each in {0,1,2}
(reach R)
(expect R 9)

; a named alt restricted to one event: only x moves
(alt X ix)
(reach RX saturate X)
(expect RX 3)

; seq: two increments as one atomic event — from x=0 only x=2 is added
; (at x=1 or 2 the first factor refuses, at 0 both fire: 0 -> 2)
(seq XX ix ix)
(reach RXX naive XX)
(expect RXX 2)

; an inline nested term behaves identically to its named parts
(reach RN (alt ix (seq ix ix)))
(expect RN 3)

; a statically dead event is the zero term: nothing in an alt …
(event dead (when (== 0 1)) (do (+= x 1)))
(alt XD ix dead)
(reach RXD XD)
(expect RXD 3)

; … and death in a seq: only the initial word remains
(seq SD ix dead)
(reach RSD SD)
(expect RSD 1)
