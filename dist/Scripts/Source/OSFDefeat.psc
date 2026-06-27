Scriptname OSFDefeat Native Hidden

; =============================================================================
; OSF Defeat - public Papyrus API.
;
; Native functions provided by "OSF Defeat.dll". OSF Defeat owns the defeat
; lifecycle (don't-die, combat suppression, recovery); your mod decides WHEN to
; defeat and WHAT happens. Detect your own trigger (e.g. RegisterForHitEvent on the
; player + a Health threshold, like SAF_Surrender) and call DefeatActor.
;
; A defeated actor is HELD (it does not auto-recover) until you call RescueActor,
; so your scene/outcome controls the timing.
; =============================================================================

; --- Defeat verbs ------------------------------------------------------------

; Put akActor into the held defeat state. Queued; returns True if accepted.
;   player : made Essential + combat-AI gate suppressed (death-alternative).
;   NPC    : bleedout life-state + ghost (invulnerable) + removed from combat.
Bool Function DefeatActor(Actor akActor) global native

; Bring a defeated actor back up (heal, restore combat, end the hold). Queued.
Bool Function RescueActor(Actor akActor, Bool abUndoPacify = True) global native

; Remove an actor from combat at the manager level (the per-actor pacify lever).
Bool Function PacifyActor(Actor akActor) global native

; True if akActor is currently in the framework defeat state.
Bool Function IsDefeated(Actor akActor) global native

; --- Classification ----------------------------------------------------------

; Classify an actor with the framework's encounter taxonomy. Useful on the captor
; to branch behaviour (e.g. Security -> arrest).
;   0 = Hostile, 1 = Crew, 2 = Security, 3 = Civilian, 4 = Unknown
Int Function ClassifyActor(Actor akActor) global native

; Current Health of akActor as a fraction of its max [0..1] - threshold a surrender
; without needing the Health actor value form.
Float Function GetHealthPercent(Actor akActor) global native

; --- Player surrender trigger (optional) -------------------------------------

; Configure the framework's built-in player surrender trigger: it downs the player
; (entering the defeat lifecycle) when an in-combat hit leaves Health below this
; FRACTION of max [0..1]. 0 disables it (default). Use this OR your own OnHit.
Function SetPlayerSurrenderThreshold(Float afFraction) global native
Float Function GetPlayerSurrenderThreshold() global native
