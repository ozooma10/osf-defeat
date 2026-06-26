Scriptname OSFDefeat Hidden
{ Public API for the OSF Defeat foundation. Compile this into your content mod's source.
  Bound by src/Papyrus/Functions.cpp. }

; ===========================================================================
; Phase 1 - LIVE (bound today)
; ===========================================================================

; State verbs. Marshaled onto the game thread; the returned bool means
; "request accepted/queued", not "completed".
bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
bool Function RescueActor(Actor akVictim, bool abUndoPacify = true) global native

; Queries. Read-only.
bool Function IsDefeated(Actor akActor) global native
Actor[] Function GetDefeated(bool abLoadedOnly = false) global native

; Classifies the DOWNED actor: "Hostile" | "Crew" | "Security" | "Civilian" | "Unknown".
; Branch on the AGGRESSOR via base-game GetRace()/IsGuard() on the ref instead.
String Function ClassifyActor(Actor akVictim, Actor akAggressor = None) global native

; Guard seam: suspend defeat processing around your own scripted scene, then re-enable.
Function DisableProcessing(bool abDisable) global native

; ===========================================================================
; Later phases - NOT YET BOUND (declared for reference; do not call yet)
; ===========================================================================
; Phase 6 - first-class Pacify (hold combat-neutral without defeating):
;   bool Function PacifyActor(Actor akActor, float afSeconds = -1.0) global native
;   Function ReleaseActor(Actor akActor) global native
;   bool Function IsPacified(Actor akActor) global native
;
; Phase 1 (after the SendEvent spike) - the defeat event:
;   Function RegisterForDefeat(Form akReceiver) global native
;   Function UnregisterForDefeat(Form akReceiver) global native
;   ; Event OnActorDefeated(Actor akVictim, Actor akAggressor, String asEncounterClass)
;   ; Event OnActorRescued(Actor akVictim, Actor akAggressor)
