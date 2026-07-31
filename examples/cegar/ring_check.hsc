; Driver for ring_model.hsc: mutual exclusion by the single token —
; two stations critical at once ((== mon 2)) is unreachable. The full
; round trip: prove, export the certificate, re-check it with the
; trusted core, and cross-check the state count explicitly.
; (Writes ring_proof.hsc beside the working directory.)
(input ring_model.hsc)

(cegar v (== mon 2))
(expect v 0)
(certificate ring_proof.hsc)
(certcheck ring_proof.hsc)
(xreach x)             ; token at N places, idle or critical: 2N states
(expect x 10)
