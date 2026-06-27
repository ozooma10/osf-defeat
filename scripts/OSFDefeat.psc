Scriptname OSFDefeat Native Hidden

; OSF Defeat - public Papyrus API foundation.
;
; Alpha adds player defeat/recovery notifications and debug helpers. Receivers
; registered below should implement OnPlayerDefeated() and/or OnPlayerRecovered().

Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native

Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native

Bool Function IsDefeated(Actor akActor) global native
Bool Function IsPacified(Actor akActor) global native
Actor[] Function GetDefeated(Bool abLoadedOnly = False) global native

Function DisableProcessing(Bool abDisable) global native
Bool Function IsProcessingDisabled() global native

Function RegisterForPlayerDefeated(Form akReceiver) global native
Function UnregisterForPlayerDefeated(Form akReceiver) global native
Function RegisterForPlayerRecovered(Form akReceiver) global native
Function UnregisterForPlayerRecovered(Form akReceiver) global native

Bool Function DebugDefeatPlayer() global native
Bool Function DebugRecoverPlayer() global native
Function ReloadJSON() global native