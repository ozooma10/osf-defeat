Scriptname OSFDefeat Native Hidden

; OSF Defeat - public Papyrus API foundation.
;
; Receivers registered below should implement OnPlayerDefeated(),
; OnPlayerConsequenceStarted(), and/or OnPlayerRecovered(). Consequence-start
; receivers can query the current payload through GetLastPlayer* functions.

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
Function RegisterForPlayerConsequenceStarted(Form akReceiver) global native
Function UnregisterForPlayerConsequenceStarted(Form akReceiver) global native

Actor Function GetLastPlayerAggressor() global native
String Function GetLastPlayerConsequenceName() global native
String Function GetLastPlayerConsequenceQuest() global native
Float Function GetLastPlayerRecoverySeconds() global native

Function ReloadJSON() global native