; Driver for smokers3_correct_assembly_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input smokers3_correct_assembly_model.hsc)
(hotbit 17 100000) (decompose-louvain) (reorder-force)
(cegar v (== mon 6))
(xreach x)
