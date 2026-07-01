Scriptname OSFDefeat Native Hidden

; OSF Defeat - public Papyrus API foundation.
;
; Events are delivered as CALLBACKS, not Papyrus events.
; Register your script plus the name of a regular Function to call back; keep the returned token to Unregister.
; Your callback is an ordinary Function you implement, e.g.:
;
;   Int property DefeatToken auto hidden
;   Event OnInit()
;       DefeatToken = OSFDefeat.RegisterForActorDefeated(Self, "OnActorDefeated")
;   EndEvent
;   Function OnActorDefeated(Actor akVictim, Actor akAggressor)
;       ; ... your consequence routing ...
;   EndFunction
;
; Callback signatures:
;   Player (no args; read context via GetLastPlayer*):
;     Function OnPlayerDefeated()
;     Function OnPlayerRecovered()
;     Function OnPlayerConsequenceStarted()
;   Actor (player + NPC):
;     Function OnActorDefeated(Actor akVictim, Actor akAggressor)
;     Function OnActorRecovered(Actor akVictim, Actor akAggressor)

Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native

Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native

; Recovery handshake mods: take ownership of a down-state or reschedule it.
; afSeconds <= 0 holds the actor down until an explicit RescueActor (content owns the exit).
; GetRecoverySecondsRemaining: > 0 counting down, -1 held (awaiting rescue), 0 not defeated.
Bool Function SetRecoveryTimer(Actor akActor, Float afSeconds) global native
Float Function GetRecoverySecondsRemaining(Actor akActor) global native

; Captive-faction pacify on demand (surrender-to-base): adds the actor to the configured captiveFaction so ALL hostiles treat them as non-hostile.
Bool Function ApplyCaptiveFaction(Actor akActor) global native
Bool Function RemoveCaptiveFaction(Actor akActor) global native
Bool Function IsCaptiveFactionApplied(Actor akActor) global native

Bool Function IsDefeated(Actor akActor) global native
Bool Function IsPacified(Actor akActor) global native
Actor[] Function GetDefeated(Bool abLoadedOnly = False) global native

; Classification for NPC-consequence routing. ClassifyActor: 0 = flesh, 1 = robot, 2 = turret.
Int Function ClassifyActor(Actor akActor) global native
Bool Function IsNonFlesh(Actor akActor) global native
Bool Function IsHostile(Actor akActor, Actor akTarget) global native

Function DisableProcessing(Bool abDisable) global native
Bool Function IsProcessingDisabled() global native

; Player events. Register returns a token; pass it to Unregister.
Int Function RegisterForPlayerDefeated(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerDefeated(Int aiToken) global native
Int Function RegisterForPlayerRecovered(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerRecovered(Int aiToken) global native
Int Function RegisterForPlayerConsequenceStarted(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerConsequenceStarted(Int aiToken) global native

; Any-actor defeat/recover events (player + NPC).
Int Function RegisterForActorDefeated(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForActorDefeated(Int aiToken) global native
Int Function RegisterForActorRecovered(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForActorRecovered(Int aiToken) global native

Actor Function GetLastPlayerAggressor() global native
String Function GetLastPlayerConsequenceName() global native
String Function GetLastPlayerConsequenceQuest() global native
Float Function GetLastPlayerRecoverySeconds() global native

; The fight's actors at defeat time: the aggressor plus recent attackers still live and hostile (recorded from actual damage events; radius-filtered by participantScanRadius).
Actor[] Function GetLastPlayerParticipants() global native

Function ReloadJSON() global native