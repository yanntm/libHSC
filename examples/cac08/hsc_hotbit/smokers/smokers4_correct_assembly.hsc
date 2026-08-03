; Driver for smokers4_correct_assembly_model.hsc — a CAC08 subject: can the property DFA err?
; Expectations are pinned by the campaign after a verified run.
(input smokers4_correct_assembly_model.hsc)
(hotbit 17 100000) (decompose-louvain) (reorder-force)
(cegar v (== mon 10))
(xreach x)
