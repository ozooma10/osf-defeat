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

## Player Events

```papyrus
Function RegisterForPlayerDefeated(Form akReceiver) global native
Function UnregisterForPlayerDefeated(Form akReceiver) global native
Function RegisterForPlayerRecovered(Form akReceiver) global native
Function UnregisterForPlayerRecovered(Form akReceiver) global native
```

Registered forms receive no-argument events:

```papyrus
Event OnPlayerDefeated()
EndEvent

Event OnPlayerRecovered()
EndEvent
```

## Debug Helpers

```papyrus
Bool Function DebugDefeatPlayer() global native
Bool Function DebugRecoverPlayer() global native
Function ReloadJSON() global native
```

These are alpha test hooks for in-game validation. `ReloadJSON()` reloads validation and consequence files without restarting the game.
## Player Consequence Handoff

```papyrus
Function RegisterForPlayerConsequenceStarted(Form akReceiver) global native
Function UnregisterForPlayerConsequenceStarted(Form akReceiver) global native
Actor Function GetLastPlayerAggressor() global native
String Function GetLastPlayerConsequenceName() global native
String Function GetLastPlayerConsequenceQuest() global native
Float Function GetLastPlayerRecoverySeconds() global native
```

Registered forms receive:

```papyrus
Event OnPlayerConsequenceStarted()
EndEvent
```

Use this event for content mods such as OSF Surrender. The event has no direct parameters; content reads the payload through the `GetLastPlayer*` functions so the native event broadcaster stays simple and stable.