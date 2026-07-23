; Parametric mutex, after ~/git/ITS-Exercise/MutexParam.gal. Each process
; walks p1 (idle) → p2 → p3; entering the critical section consumes every
; other process's idle token, leaving frees them all. The GAL original
; cannot state this start: it boots from the encoding of "someone is in
; the section" (pSC=1, all places empty) and relies on a firing of tendSC
; to reach the intended start — the init event states it directly.
; "all j except i" is two dependent ranges: j in [0,i) and j in [i+1,N).
(param N 4)
(array p1 N 0 2)
(array p2 N 0 2)
(array p3 N 0 2)
(leaf pSC 0 2)
(shape (spine (forall (i N) (at p1 i) (at p2 i) (at p3 i)) pSC))
(init (forall (i N) (do (:= (at p1 i) 1))))
(event t1
  (exists (i N) (when (== (at p1 i) 1))
    (do (:= (at p1 i) 0) (:= (at p2 i) 1))))
(event t2
  (exists (i N) (when (== (at p2 i) 1))
    (do (:= (at p2 i) 0) (:= (at p3 i) 1))))
(event tSC
  (exists (i N)
    (when (== (at p3 i) 1)
          (forall (j i) (>= (at p1 j) 1))
          (forall (j (+ i 1) N) (>= (at p1 j) 1)))
    (do (:= (at p3 i) 0) (+= pSC 1)
        (forall (j i) (-= (at p1 j) 1))
        (forall (j (+ i 1) N) (-= (at p1 j) 1)))))
(event tendSC
  (when (>= pSC 1))
  (do (-= pSC 1) (forall (i N) (+= (at p1 i) 1))))
(reach R saturate)
(count R)
(reach RN naive)
(expect RN 82)
(expect R 82)
; mutual exclusion: no state holds the section (pSC=1) while any process
; still waits at p3 with the tokens to contest it — the image is empty
(apply clash
  (when (>= pSC 1) (exists (i N) (>= (at p3 i) 1)))
  R)
(expect clash 0)
