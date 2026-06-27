Scriptname OSFDefeat Native Hidden

; OSF Defeat - public Papyrus API foundation.
;
; This script exposes the durable framework verbs. It deliberately does not own
; trigger detection, resolution selection, or authored consequences yet. Content
; mods decide when to call DefeatActor and when to call RescueActor.

Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native

Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native

Bool Function IsDefeated(Actor akActor) global native
Bool Function IsPacified(Actor akActor) global native
Actor[] Function GetDefeated(Bool abLoadedOnly = False) global native

Function DisableProcessing(Bool abDisable) global native
Bool Function IsProcessingDisabled() global native
