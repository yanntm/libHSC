; leaf_weight: a leaf that stands for several places.
;
; A structural reduction may fuse a free component of a net -- K places over
; which tokens travel freely -- into one place holding the total. Every
; distribution of that total is reachable, so a marking of v in the fused
; place represents C(v+K-1, K-1) markings of the original net, and counting
; folds that binomial in at the leaf.
;
; Here x is declared to stand for 3 places, y for itself. The reachable set
; is x in {0,1,2} times y in {0,1}, six markings of this net; of the original
; net they are (1 + 3 + 6) * 2 = 20, since C(2,2)=1, C(3,2)=3, C(4,2)=6.
(leaf x 0 5)
(leaf y 0 5)
(shape (spine x y))
(init (seq (do (havoc x 0 3)) (do (havoc y 0 2))))
(reach R saturate)
(count R)                ; the diagram's own six markings, unweighted
(leaf-weight x 3)
(count R exact)          ; 20, the markings of the net this one came from
(expect R 20)
