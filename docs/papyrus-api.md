# OSF Defeat Papyrus API

`OSFDefeat.psc` is the public alpha API. Calls that mutate game state are marshaled to the game thread by the native plugin. Their `Bool` return value means the request was accepted for dispatch, not that the deferred operation succeeded. Confirm state with the matching query/callback before starting a dependent transition (for example, wait until `IsDefeated(actor)` is false before handing the player to another quest pipeline).

## Verbs

```papyrus
Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native
Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native
```

`DefeatActor` records the actor as defeated and persists that state. For the player, it also applies the native down state. `RescueActor` removes defeat state and restores the player if needed. `PacifyActor` / `ReleaseActor` are foundation-level state hooks for future combat behavior.

## Recovery Handshake

```papyrus
Bool Function SetRecoveryTimer(Actor akActor, Float afSeconds) global native
Float Function GetRecoverySecondsRemaining(Actor akActor) global native
```

This is how a content mod takes ownership of a down-state. `SetRecoveryTimer(actor, 0.0)` (or any `afSeconds <= 0`) **holds** the actor down — the framework's auto-recovery skips them until you call `RescueActor` (or set a positive timer again). A positive value reschedules auto-recovery that many real seconds out. `GetRecoverySecondsRemaining` returns the remaining real seconds (`-1` = held awaiting explicit rescue, `0` = not defeated).

A consequence JSON can also ship `"holdRecovery": true` so the player enters the down-state already held — no race against the recovery timer.

## Ownership Lease + Watchdog

```papyrus
Bool Function ClaimDefeat(Actor akActor, String asOwner, Float afMaxHoldSeconds = 0.0) global native
Bool Function ReleaseClaim(Actor akActor, String asOwner) global native
String Function GetClaimOwner(Actor akActor) global native
```

The formal version of the recovery handshake. `ClaimDefeat` holds the down-state under your
mod's name with a **watchdog deadline** (`afMaxHoldSeconds <= 0` uses the configured
`watchdogSeconds`, default 600). If the deadline lapses — your quest stopped, errored out, or
forgot to finish — the framework force-rescues the actor and logs your name as the defaulter,
so a broken consumer can never strand the player. Rules:

- Re-claim with the **same** name to extend the deadline before it lapses (long scenes should
  re-claim periodically, or pass a generous `afMaxHoldSeconds` up front).
- A claim against a **different** live lease is refused (two content packs cannot fight over
  one down-state). Claims are queued to the game thread; confirm with `GetClaimOwner`.
- `ReleaseClaim` (name must match) ends the lease and resumes the default recovery timer.
- `RescueActor` always works, lease or not.
- A consequence with `"holdRecovery": true` is auto-claimed under the consequence's name, with
  its `"maxHoldSeconds"` (or the config default) as deadline — JSON packs get watchdog
  protection without writing a line of script.
- The player's held down-states are **always** watchdog-protected, even without a claim. NPC
  held entries are only deadline-checked when leased — a held NPC captive is a legitimate
  long-term state.

## Emergency Escape / Safe Uninstall

```papyrus
Bool Function ForceRecoverAll() global native
```

Requests recovery for every resolvable defeated actor (overriding any lease), releases every
resolvable pacified actor, and strips the captive-faction memberships the framework created.
Unresolved pacify/faction ownership records are retained and logged so they can be cleaned if the
actor becomes resolvable later. Verify the log/state, then save; once no records remain, the next
co-save is empty and the DLL can be removed. Users can run it from the
console any time they feel stuck:

```
cgf "OSFDefeat.ForceRecoverAll"
```

## Captive-Faction Pacify (surrender-to-base)

```papyrus
Bool Function ApplyCaptiveFaction(Actor akActor) global native
Bool Function RemoveCaptiveFaction(Actor akActor) global native
Bool Function IsCaptiveFactionApplied(Actor akActor) global native
```

Adds the actor to the configured `captiveFaction` (default: vanilla `0x0003E0C8`) so **all** hostiles treat them as non-hostile — the full SAF Surrender-style calm, on demand, independent of the automatic `factionPacify` config gate. Guarded per actor: the membership is added at most once, never over a pre-existing membership, and removal deletes exactly (and only) the membership the framework created. It is removed automatically on `RescueActor`; the tracking survives save/load via the co-save.

## Queries

```papyrus
Bool Function IsDefeated(Actor akActor) global native
Bool Function IsPacified(Actor akActor) global native
Actor[] Function GetDefeated(Bool abLoadedOnly = False) global native
Int Function GetDefeatCause(Actor akActor) global native
Int Function GetLastPlayerDefeatCause() global native
```

`GetDefeatCause` says **how** an actor went down, so content can route a scripted/capture defeat
differently from a beaten one: `0` none/not defeated, `1` lethal Health damage, `2` scripted (the
`DefeatActor` verb / capture). Persisted in the co-save.
`GetLastPlayerDefeatCause` is the same value in the player consequence handoff context.

## Struggle Primitives (defeat imminent + resist)

```papyrus
Int  Function RegisterForPlayerDefeatImminent(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerDefeatImminent(Int aiToken) global native
Bool Function ResistDefeat(Actor akActor, Float afSeconds) global native
Bool Function IsResistingDefeat(Actor akActor) global native
```

The framework exposes the *moment*; content builds the minigame. `OnPlayerDefeatImminent` fires
once when a Health hit takes the player below `imminentHealthFraction` × max health (validation
JSON, default `0.25`, `0` disables); it re-arms after healing back above the threshold, and after
a defeat. Use it for struggle QTEs, surrender prompts, or last-stand effects.

`ResistDefeat(actor, seconds)` opens a **no-defeat grace window**: while active, would-be-lethal
hits pass through the clamp untouched — **vanilla lethality returns**, the actor fights on and
can genuinely die. That is the intended tension of a successful struggle roll. `afSeconds <= 0`
clears the window; calling it on an already-defeated actor is refused (a resist window is not a
rescue — use `RescueActor`). Works on NPCs too (e.g. a boss that shrugs off the first defeat).

## Processing Gate

```papyrus
Function DisableProcessing(Bool abDisable) global native
Bool Function IsProcessingDisabled() global native
```

Use this around scripted scenes or other content that wants vanilla death/behavior to proceed without OSF Defeat handling.

## Events are callbacks

Starfield Papyrus cannot deliver *events* to non-native scripts, so OSF Defeat uses **callbacks** (the same mechanism as OSF Animation): you register your script plus the name of a regular `Function` to call, and get back an `Int` token to unregister with. Your handler is an ordinary `Function`, not an `Event`.

```papyrus
Int  Function RegisterForActorDefeated(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForActorDefeated(Int aiToken) global native
```

## Player Events

```papyrus
Int  Function RegisterForPlayerDefeated(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerDefeated(Int aiToken) global native
Int  Function RegisterForPlayerRecovered(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerRecovered(Int aiToken) global native
```

The callbacks take no arguments:

```papyrus
Event OnInit()
    OSFDefeat.RegisterForPlayerDefeated(Self, "OnPlayerDefeated")
EndEvent

Function OnPlayerDefeated()
EndFunction

Function OnPlayerRecovered()
EndFunction
```

## Classification

```papyrus
Int Function ClassifyActor(Actor akActor) global native ; 0 = flesh, 1 = robot, 2 = turret
Bool Function IsNonFlesh(Actor akActor) global native
Bool Function IsHostile(Actor akActor, Actor akTarget) global native
```

`ClassifyActor` reads the actor's ActorType keywords; `IsHostile` checks combat hostility. Safe to call inline.

## Utilities

```papyrus
Function ReloadJSON() global native
```

`ReloadJSON()` reloads validation, consequence, and override files without restarting the game.
Pair it with `"explainDecisions": true` (validation JSON) for a live authoring loop: edit JSON,
reload, defeat, and read the selection log — every filtered outcome logs its reason (disabled,
off-domain, cooling down, spent for this location, aggressor/victim conditions failed) plus the
final priority/weight roll.
## Player Consequence Handoff

```papyrus
Int  Function RegisterForPlayerConsequenceStarted(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerConsequenceStarted(Int aiToken) global native
Actor Function GetLastPlayerAggressor() global native
String Function GetLastPlayerConsequenceName() global native
String Function GetLastPlayerConsequenceQuest() global native
Float Function GetLastPlayerRecoverySeconds() global native
Actor[] Function GetLastPlayerParticipants() global native
```

The callback takes no arguments:

```papyrus
Function OnPlayerConsequenceStarted()
EndFunction
```

Use this for content mods such as OSF Surrender. The callback has no direct parameters; content reads the payload through the `GetLastPlayer*` functions so the broadcaster stays simple and stable.

`GetLastPlayerParticipants` returns the fight's cast at defeat time: the aggressor plus everyone who damaged the player in the last ~30 seconds and is still a live hostile — scene casting for group content. `participantScanRadius` (validation JSON, default 4096 units, `0` = no radius filter) additionally drops distant entries. The list is recorded from actual damage events, so it is combatants, not bystanders.

### Consequence quest start

If a selected consequence carries a `quest` token that resolves to a real quest — by **editorID** first, then as a hex FormID (`"0x00012345"`) — the framework dispatches `Quest.Start` on it before broadcasting `OnPlayerConsequenceStarted`, so the quest is already running when the callback fires. A token that resolves to nothing stays a plain content string (previous behavior). Per-consequence `"startQuest": false` opts out of the auto-start.

## Actor Events (player + NPC)

```papyrus
Int  Function RegisterForActorDefeated(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForActorDefeated(Int aiToken) global native
Int  Function RegisterForActorRecovered(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForActorRecovered(Int aiToken) global native
```

Unlike the player events, these fire for **any** defeated actor — player and NPC — and the callbacks carry the victim and aggressor:

```papyrus
Function OnActorDefeated(Actor akVictim, Actor akAggressor)
EndFunction

Function OnActorRecovered(Actor akVictim, Actor akAggressor)
EndFunction
```

### NPC consequence content (consumer recipe)

OSF Defeat owns the neutral defeat state; **content mods own what a defeat means**. To author NPC consequences, register a callback, classify the victim, and run your own scene/outcome:

```papyrus
Event OnInit()
    OSFDefeat.RegisterForActorDefeated(Self, "OnActorDefeated")
EndEvent

Function OnActorDefeated(Actor akVictim, Actor akAggressor)
    If akVictim == Game.GetPlayer()
        Return ; the player path has its own consequence handoff
    EndIf
    Int classification = OSFDefeat.ClassifyActor(akVictim)
    If classification == 0 ; flesh
        ; run capture / loot / ransom against akVictim, attributed to akAggressor
    Else
        ; robot / turret: salvage, disable, etc.
    EndIf
EndFunction
```

Recovery: an actor downed via `DefeatActor` is **held** until you call `RescueActor`, so content fully controls its lifetime. An auto-defeated NPC (the opt-in `npcAutoDefeat`) carries the default recovery timer instead — defeat it yourself via `DefeatActor` if you need a longer hold.
