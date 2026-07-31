; Driver for clients_model.hsc: prove "no grant on an outstanding
; grant" and cross-check the concrete count explicitly. Expectations
; are for the model's default K = 4.
(input clients_model.hsc)

(cegar v (== mon 2))   ; holds: v binds empty
(expect v 0)
(xreach x)             ; the concrete oracle: free + K busy states
(expect x 5)
