; Driver for peterson3_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input peterson3_model.hsc)
(cegar v (== mon 4))
(xreach x)
(expect v 0)
(expect x 2857)
