; Driver for relay_03_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input relay_03_model.hsc)
(cegar v (== mon 2))
(xreach x)
