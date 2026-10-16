# OSF Defeat Papyrus API

`OSFDefeat.psc` is the public alpha API. Calls that mutate game state use the native engine task queue. They may execute inline when already on its drain owner; otherwise they run in a later drain. Their `Bool` return value means the request was accepted for dispatch, not that the operation succeeded. Confirm state with the matching query/callback before starting a dependent transition (for example, wait until `IsDefeated(actor)` is false before handing the player to another quest pipeline).

## Verbs

```papyrus
Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RecoverActor(Actor akActor, Bool abResumeAI = True) global native
Bool Function SuspendActorAI(Actor akActor) global native
Bool Function ResumeActorAI(Actor akActor) global native
```

`DefeatActor` records the actor as defeated and persists that state. `RecoverActor` ends the defeat and restores framework-owned actor state. `SuspendActorAI` records the actor's prior AI-processing state before disabling it; `ResumeActorAI` restores that exact state.

## Recovery Handshake

```papyrus
Bool Function ScheduleAutomaticRecovery(Actor akActor, Float afDelaySeconds) global native
Bool Function HoldRecovery(Actor akActor) global native
Float Function GetAutomaticRecoverySecondsRemaining(Actor akActor) global native
```

This is how a content mod schedules or holds recovery. `HoldRecovery` creates a **recovery hold**: automatic recovery skips the actor until you call `RecoverActor`, release a lease, or schedule it again. `ScheduleAutomaticRecovery` requires a positive real-time delay and ends any lease. `GetAutomaticRecoverySecondsRemaining` returns the remaining real seconds (`-1` = held awaiting explicit recovery, `0` = not defeated).

A consequence JSON can use `"recoveryMode": "held"` so the player enters the down-state already held, with no race against an automatic timer.

## Defeat Lease + Watchdog

```papyrus
Bool Function AcquireDefeatLease(Actor akActor, String asHolderId, Float afTimeoutSeconds = 0.0) global native
Bool Function ReleaseDefeatLease(Actor akActor, String asHolderId) global native
String Function GetDefeatLeaseHolder(Actor akActor) global native
```

The exclusive form of the recovery handshake. `AcquireDefeatLease` holds the down-state under your
mod's stable holder ID with a **watchdog deadline** (`afTimeoutSeconds <= 0` uses the configured
`watchdogSeconds`, default 600). If the deadline lapses, the framework force-recovers the actor
and logs the holder ID as the defaulter,
so a broken consumer can never strand the player. Rules:

- Acquire with the **same** holder ID again to extend the deadline before it lapses.
- Acquisition by a **different** holder is refused. Calls are queued to the game thread; confirm with `GetDefeatLeaseHolder`.
- `ReleaseDefeatLease` (holder ID must match) ends the lease and resumes the default recovery timer.
- `RecoverActor` always works, lease or not.
- A consequence with `"recoveryMode": "held"` automatically leases under `consequence:<id>`, with
  `"leaseTimeoutSeconds"` (or the settings default) as its deadline, so JSON packs get watchdog
  protection without writing a line of script.
- The player's held down-states are **always** watchdog-protected, even without a lease. NPC
  held entries are deadline-checked only when leased; a held NPC captive is a legitimate
  long-term state.

## Emergency Escape / Safe Uninstall

```papyrus
Bool Function ForceRecoverAll() global native
```

Requests recovery for every resolvable defeated actor (overriding any lease), resumes AI for every
resolvable actor whose AI the framework suspended, and removes captive-faction memberships it created.
Unresolved AI-suspension/faction ownership records are retained and logged so they can be cleaned if the
actor becomes resolvable later. Verify the log/state, then save; once no records remain, the next
co-save is empty and the DLL can be removed. Users can run it from the
console any time they feel stuck:

```
cgf "OSFDefeat.ForceRecoverAll"
```

## Captive-Faction Non-Hostility (surrender-to-base)

```papyrus
Bool Function ApplyCaptiveFaction(Actor akActor) global native
Bool Function RemoveCaptiveFaction(Actor akActor) global native
Bool Function IsCaptiveFactionApplied(Actor akActor) global native
```

Adds the actor to the configured `captiveFaction` (default: vanilla `0x0003E0C8`) so **all** hostiles treat them as non-hostile. This is distinct from AI suspension and is independent of `applyCaptiveFactionOnPlayerDefeat`. Membership is added at most once, never over a pre-existing membership, and removal deletes only membership the framework created. It is removed automatically on `RecoverActor`; tracking survives save/load via the co-save.

## Queries

```papyrus
Bool Function IsDefeated(Actor akActor) global native
Bool Function IsActorAISuspended(Actor akActor) global native
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
once when a Health hit takes the player below `imminentHealthFraction` × max health (settings
JSON, default `0.25`, `0` disables); it re-arms after healing back above the threshold, and after
a defeat. Use it for struggle QTEs, surrender prompts, or last-stand effects.

`ResistDefeat(actor, seconds)` opens a **no-defeat grace window**: while active, would-be-lethal
hits pass through the clamp untouched — **vanilla lethality returns**, the actor fights on and
can genuinely die. That is the intended tension of a successful struggle roll. `afSeconds <= 0`
clears the window; calling it on an already-defeated actor is refused (a resist window is not a
recovery — use `RecoverActor`). Works on NPCs too (e.g. a boss that shrugs off the first defeat).

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

`ReloadJSON()` reloads settings, consequence, and override files without restarting the game.
Pair it with `"explainDecisions": true` (settings JSON) for a live authoring loop: edit JSON,
reload, defeat, and read the selection log — every filtered consequence logs its reason (disabled,
off-domain, cooling down, spent for this location, aggressor/victim conditions failed) plus the
final priority/weight roll.
## Player Consequence Handoff

```papyrus
Int  Function RegisterForPlayerConsequenceStarted(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerConsequenceStarted(Int aiToken) global native
Actor Function GetLastPlayerAggressor() global native
String Function GetLastPlayerConsequenceId() global native
String Function GetLastPlayerConsequenceName() global native
String Function GetLastPlayerConsequenceQuest() global native
Float Function GetLastPlayerAutoRecoveryDelaySeconds() global native
Bool Function IsLastPlayerRecoveryHeld() global native
Actor[] Function GetLastPlayerRecentHostileAttackers() global native
```

The callback takes no arguments:

```papyrus
Function OnPlayerConsequenceStarted()
EndFunction
```

Use this for content mods such as OSF Surrender. The callback has no direct parameters; content reads the payload through the `GetLastPlayer*` functions so the broadcaster stays simple and stable.

`GetLastPlayerConsequenceId` is the stable machine identity; `GetLastPlayerConsequenceName` is the display label retained for compatibility. `GetLastPlayerRecentHostileAttackers` returns the aggressor plus recent damage sources that are still live and hostile. `recentHostileAttackerRadius` (settings JSON, default 4096 units, `0` = no radius filter) drops distant entries. The list comes from actual damage events, so it contains combatants rather than bystanders.

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

OSF Defeat owns the neutral defeat state; **content mods own what a defeat means**. To author NPC consequences, register a callback, classify the victim role, and run your own content:

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

Recovery: an actor downed via `DefeatActor` is **held** until you call `RecoverActor`, so content fully controls its lifetime. An auto-defeated NPC (the opt-in `npcAutoDefeat`) carries the default automatic recovery delay instead; defeat it yourself via `DefeatActor` if you need a longer hold.

## Legacy aliases

Existing compiled consumers remain supported. `RescueActor`, `PacifyActor`, `ReleaseActor`, `IsPacified`, `ClaimDefeat`, `ReleaseClaim`, `GetClaimOwner`, `SetRecoveryTimer`, `GetRecoverySecondsRemaining`, `GetLastPlayerParticipants`, and `GetLastPlayerRecoverySeconds` forward to the canonical APIs. New integrations should use the canonical names above. See [the vocabulary contract](VOCABULARY.md) for the complete JSON and directory compatibility map.
