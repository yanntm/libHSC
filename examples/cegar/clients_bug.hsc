; Driver for clients_bug_model.hsc: the loop finds the double grant,
; validates it by leaf executions, and binds the bad state.
(input clients_bug_model.hsc)

(cegar v (== mon 2))   ; violation: v binds the validated bad state
(expect v 1)
(get-witness v)
