; Driver for gas_c005_correct_change_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input gas_c005_correct_change_model.hsc)
(cegar v (== mon 2))
(xreach x)
