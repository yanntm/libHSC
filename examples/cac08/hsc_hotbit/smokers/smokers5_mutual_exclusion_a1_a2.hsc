; Driver for smokers5_mutual_exclusion_a1_a2_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input smokers5_mutual_exclusion_a1_a2_model.hsc)
(hotbit 17 100000) (decompose-louvain) (reorder-force)
(cegar v (== mon 3))
(xreach x)
