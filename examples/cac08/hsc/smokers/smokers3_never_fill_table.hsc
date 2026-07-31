; Driver for smokers3_never_fill_table_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input smokers3_never_fill_table_model.hsc)
(cegar v (== mon 3))
(xreach x)
(expect v 0)
(expect x 116)
