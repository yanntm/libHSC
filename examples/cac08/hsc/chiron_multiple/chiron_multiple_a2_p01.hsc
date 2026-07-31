; Driver for chiron_multiple_a2_p01_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input chiron_multiple_a2_p01_model.hsc)
(cegar v (== mon 2))
(xreach x)
(expect v 0)
(expect x 794)
