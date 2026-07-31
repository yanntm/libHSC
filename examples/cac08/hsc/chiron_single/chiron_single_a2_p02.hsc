; Driver for chiron_single_a2_p02_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input chiron_single_a2_p02_model.hsc)
(cegar v (== mon 4))
(xreach x)
(expect v 0)
(expect x 137)
