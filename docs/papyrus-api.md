# OSF Defeat Papyrus API

`OSFDefeat.psc` is the public alpha API. Calls that mutate game state are marshaled to the game thread by the native plugin.

## Verbs

```papyrus
Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native
Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native
```

`DefeatActor` records the actor as defeated and persists that state. For the player, it also applies the native down state. `RescueActor` removes defeat state and restores the player if needed. `PacifyActor` / `ReleaseActor` are foundation-level state hooks for future combat behavior.

## Queries

```papyrus
Bool Function IsDefeated(Actor akActor) global native
Bool Function IsPacified(Actor akActor) global native
Actor[] Function GetDefeated(Bool abLoadedOnly = False) global native
```

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

`ReloadJSON()` reloads validation and consequence files without restarting the game.
## Player Consequence Handoff

```papyrus
Int  Function RegisterForPlayerConsequenceStarted(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerConsequenceStarted(Int aiToken) global native
Actor Function GetLastPlayerAggressor() global native
String Function GetLastPlayerConsequenceName() global native
String Function GetLastPlayerConsequenceQuest() global native
Float Function GetLastPlayerRecoverySeconds() global native
```

The callback takes no arguments:

```papyrus
Function OnPlayerConsequenceStarted()
EndFunction
```

Use this for content mods such as OSF Surrender. The callback has no direct parameters; content reads the payload through the `GetLastPlayer*` functions so the broadcaster stays simple and stable.

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