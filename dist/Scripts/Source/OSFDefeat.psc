Scriptname OSFDefeat Native Hidden

; OSF Defeat - public Papyrus API foundation.
;
; Receivers registered below should implement OnPlayerDefeated(),
; OnPlayerConsequenceStarted(), and/or OnPlayerRecovered(). Consequence-start
; receivers can query the current payload through GetLastPlayer* functions.
;
; Actor-event receivers (RegisterForActorDefeated/Recovered) fire for ANY defeated
; actor — player and NPC — and should implement:
;   Event OnActorDefeated(Actor akVictim, Actor akAggressor)
;   Event OnActorRecovered(Actor akVictim, Actor akAggressor)

Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native

Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native

Bool Function IsDefeated(Actor akActor) global native
Bool Function IsPacified(Actor akActor) global native
Actor[] Function GetDefeated(Bool abLoadedOnly = False) global native

; Classification for NPC-consequence routing. ClassifyActor: 0 = flesh, 1 = robot, 2 = turret.
Int Function ClassifyActor(Actor akActor) global native
Bool Function IsNonFlesh(Actor akActor) global native
Bool Function IsHostile(Actor akActor, Actor akTarget) global native

Function DisableProcessing(Bool abDisable) global native
Bool Function IsProcessingDisabled() global native

Function RegisterForPlayerDefeated(Form akReceiver) global native
Function UnregisterForPlayerDefeated(Form akReceiver) global native
Function RegisterForPlayerRecovered(Form akReceiver) global native
Function UnregisterForPlayerRecovered(Form akReceiver) global native
Function RegisterForPlayerConsequenceStarted(Form akReceiver) global native
Function UnregisterForPlayerConsequenceStarted(Form akReceiver) global native

; Any-actor defeat/recover events (player + NPC). Callback signatures above.
Function RegisterForActorDefeated(Form akReceiver) global native
Function UnregisterForActorDefeated(Form akReceiver) global native
Function RegisterForActorRecovered(Form akReceiver) global native
Function UnregisterForActorRecovered(Form akReceiver) global native

Actor Function GetLastPlayerAggressor() global native
String Function GetLastPlayerConsequenceName() global native
String Function GetLastPlayerConsequenceQuest() global native
Float Function GetLastPlayerRecoverySeconds() global native

Bool Function DebugDefeatPlayer() global native
Bool Function DebugRecoverPlayer() global native

; Force-defeat / recover an arbitrary ref by FormID (the value from Form.GetFormID()),
; bypassing the validation gate. Handy for testing NPC defeat from the console.
Bool Function DebugDefeatRef(Int aiFormID) global native
Bool Function DebugRecoverRef(Int aiFormID) global native

Function ReloadJSON() global native