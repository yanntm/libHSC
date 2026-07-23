; The state layer: an init *event* over the base word (every leaf at LO),
; havoc as a range action, words as bound results, reach from any bound
; result, one-step images, and exhibiting states. Oracles exact by hand.
(leaf x 0 10)
(leaf y 2 10)
(shape (spine x y))

; seed = the event applied ONCE to the base (x=0, y=2): alt of a constant
; state and a constrained havoc region — x in {1} ∪ ({5,6,7} \ {6}), y=2.
(init (alt (do (:= x 1))
           (seq (do (havoc x 5 8)) (when (!= x 6)))))

(event ix (when (< x 9)) (do (+= x 1)))
(event noop (when))

; the seed alone: closure of the no-op event
(reach S0 noop)
(expect S0 3)

; the default system (ix) from the seed: x in {1..9}, y=2
(reach R)
(expect R 9)

; a bound word, and reach from it: x in {3..9}
(word W (x 3) (y 4))
(reach RW from W)
(expect RW 7)

; the one-step image, no closure: x=4 exactly
(apply A ix W)
(expect A 1)

; reach from a query result: x in {8,9} is already closed under ix
(select S R (> x 7))
(reach R2 from S)
(expect R2 2)

; exhibit: one state of A (a word literal), then a bounded enumeration
(get-witness A)
(get-states R 4)
