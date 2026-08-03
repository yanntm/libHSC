; Driver for relay_09_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input relay_09_model.hsc)
(hotbit 17 100000) (decompose-louvain) (reorder-force)
(cegar v (== mon 2))
(xreach x)
