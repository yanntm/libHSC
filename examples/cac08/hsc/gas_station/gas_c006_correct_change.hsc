; Driver for gas_c006_correct_change_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input gas_c006_correct_change_model.hsc)
(cegar v (== mon 2))
(xreach x)
