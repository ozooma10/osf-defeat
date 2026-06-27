# OSF Defeat Papyrus API

`OSFDefeat.psc` is the current public content seam. The API is intentionally small while the native foundation settles.

## Verbs

```papyrus
Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native
Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native
```

`DefeatActor` records the actor as defeated and persists that state. `RescueActor` removes that state. `PacifyActor` / `ReleaseActor` are foundation-level state hooks for future combat behavior.

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

Use this around scripted scenes or other content that wants to pause framework processing.
