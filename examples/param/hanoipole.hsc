; Hanoi over occupancy bits, after ~/git/ITS-Exercise/hanoipole.gal:
; poles is a P×N 2-D array, cell (p r) = 1 iff ring r sits on pole p. The
; GAL original linearizes by hand (poles[$r + ($src*$N)]) and boots through
; an init flag and a doInit transition, dragging init==1 through every
; guard; here the 2-D access is direct and the init event marks pole 0.
; Same state space as hanoi.hsc: the bits stay one-hot per ring, 3^N.
(param N 7)
(param P 3)
(array poles P N 0 2)
(shape (spine (forall (p P) (forall (r N) (at poles p r)))))
(init (forall (r N) (do (:= (at poles 0 r) 1))))
(event move
  (exists (s P) (exists (d P) (exists (r N)
    (when (!= s d) (== (at poles s r) 1)
          (forall (o r) (and (== (at poles s o) 0) (== (at poles d o) 0))))
    (do (:= (at poles s r) 0) (:= (at poles d r) 1))))))
(reach R saturate)
(count R)
(expect R 2187)
(select goal R (forall (r N) (== (at poles (- P 1) r) 1)))
(expect goal 1)
