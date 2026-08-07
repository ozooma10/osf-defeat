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
;
; IMPORTANT: Bool mutators marshaled to the game thread return whether the REQUEST was accepted,
; not the eventual operation result. Confirm completion with IsDefeated / IsPacified /
; IsCaptiveFactionApplied / IsCaptive / GetClaimOwner, or with the matching callback.

Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native

Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native

; Recovery handshake mods: take ownership of a down-state or reschedule it.
; afSeconds <= 0 holds the actor down until an explicit RescueActor (content owns the exit).
; GetRecoverySecondsRemaining: > 0 counting down, -1 held (awaiting rescue), 0 not defeated.
Bool Function SetRecoveryTimer(Actor akActor, Float afSeconds) global native
Float Function GetRecoverySecondsRemaining(Actor akActor) global native

; Ownership lease. ClaimDefeat holds the down-state under your name with a watchdog deadline
; (afMaxHoldSeconds <= 0 uses the configured watchdogSeconds). Re-claim with the same name to
; EXTEND the deadline before it lapses; if it lapses, the framework force-rescues and logs your
; name as the defaulter. ReleaseClaim (name must match) ends the lease and resumes the default
; recovery timer. RescueActor always works regardless of any lease. The claim is queued to the
; game thread - confirm with GetClaimOwner if another mod might be competing.
Bool Function ClaimDefeat(Actor akActor, String asOwner, Float afMaxHoldSeconds = 0.0) global native
Bool Function ReleaseClaim(Actor akActor, String asOwner) global native
String Function GetClaimOwner(Actor akActor) global native

; Emergency escape / safe uninstall: requests rescue for every resolvable defeated actor (overriding
; any lease), releases owned pacifies/factions, and logs unresolved records for a later retry. Once
; no records remain, the next co-save is empty. Console: cgf "OSFDefeat.ForceRecoverAll"
Bool Function ForceRecoverAll() global native

; How an actor went down - content can route a scripted/capture defeat distinctly from a Health-down.
;   0 = none/not defeated, 1 = lethal Health damage, 2 = scripted (DefeatActor / capture)
Int Function GetDefeatCause(Actor akActor) global native
Int Function GetLastPlayerDefeatCause() global native

; Struggle primitive. RegisterForPlayerDefeatImminent fires once when a Health hit takes the
; player below imminentHealthFraction x max health (validation JSON, default 0.25; re-arms after
; healing above the threshold or after a defeat). Build your QTE/struggle beat in the callback;
; on a successful resist call ResistDefeat: while the window is active, would-be-lethal hits pass
; through untouched - VANILLA LETHALITY - the actor fights on and can genuinely die.
; afSeconds <= 0 clears the window. Refused (false) for an already-defeated actor.
Bool Function ResistDefeat(Actor akActor, Float afSeconds) global native
Bool Function IsResistingDefeat(Actor akActor) global native
Int Function RegisterForPlayerDefeatImminent(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerDefeatImminent(Int aiToken) global native

; Captive-faction pacify on demand (surrender-to-base): adds the actor to the configured captiveFaction so ALL hostiles treat them as non-hostile.
Bool Function ApplyCaptiveFaction(Actor akActor) global native
Bool Function RemoveCaptiveFaction(Actor akActor) global native
Bool Function IsCaptiveFactionApplied(Actor akActor) global native

; Capture hooks: claim a defeated NPC as a captor's prisoner - the foundation for a bounty/ransom
; board. CaptureActor puts the captive down if needed (scripted cause, akCaptor as aggressor), holds
; recovery indefinitely, applies the captive faction (hostiles ignore the prisoner), and records the
; captor (akCaptor may be None for an un-attributed hold). The capture is co-save persisted and is
; auto-cleared when the captive recovers by any path (rescue / watchdog / ForceRecoverAll).
; ReleaseCaptive ends it: abRecover also rescues (ends the down-state); otherwise the captive stays
; down but is no longer claimed. TransferCaptive moves a captive to a marker/ref (brig, ship, cell)
; via ObjectReference.MoveTo - the framework re-asserts the down-state at the destination.
; GetCaptives(None) returns every captive; pass a captor to list only theirs.
Bool Function CaptureActor(Actor akCaptive, Actor akCaptor = None) global native
Bool Function ReleaseCaptive(Actor akActor, Bool abRecover = True) global native
Bool Function TransferCaptive(Actor akActor, ObjectReference akTarget) global native
Bool Function IsCaptive(Actor akActor) global native
Actor Function GetCaptor(Actor akActor) global native
Actor[] Function GetCaptives(Actor akCaptor = None) global native

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
