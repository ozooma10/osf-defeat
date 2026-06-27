# OSF Defeat

A native **defeat framework foundation** for Starfield, inspired by Skyrim's Acheron model and built on [CommonLibSF](https://github.com/Starfield-Reverse-Engineering/CommonLibSF).

This branch is intentionally small. It provides the durable framework spine first:

- SFSE plugin load and messaging setup
- co-save persistence for defeated actors
- a thread-safe defeated-actor registry
- basic Defeat / Rescue / Pacify verbs
- a small Papyrus API content mods can call

It does **not** currently install damage hooks, built-in surrender triggers, automatic recovery, combat suppression, bleedout forcing, JSON consequences, or UI. Those pieces should be added back one at a time once the foundation is stable and each Starfield engine primitive is verified.

## Current Architecture

| Layer | Purpose |
|---|---|
| `Core/DefeatRegistry` | Thread-safe actor state store. |
| `Core/Defeat` | Acheron-style verbs: defeat, rescue, pacify, query. |
| `Core/Validation` | Global processing gate plus minimal actor validation. |
| `Core/Persistence` | SFSE co-save serialization for defeated actors. |
| `Papyrus/Functions` | Native bindings for the public `OSFDefeat` script. |

## Papyrus API

The declarations live in `dist/Scripts/Source/OSFDefeat.psc`.

```papyrus
Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native
Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native
Bool Function IsDefeated(Actor akActor) global native
Bool Function IsPacified(Actor akActor) global native
Actor[] Function GetDefeated(Bool abLoadedOnly = False) global native
Function DisableProcessing(Bool abDisable) global native
Bool Function IsProcessingDisabled() global native
```

## Build

```bat
xmake f -p windows -a x64 -m debug -y
xmake build
```

Set `XSE_SF_MODS_PATH` to your mod-manager `mods` folder, or `XSE_SF_GAME_PATH` to the game install, to auto-deploy build outputs.

## Next Foundation Steps

1. Add native event broadcasting for defeated/rescued actors.
2. Add a Starfield-safe validation/config layer.
3. Reintroduce engine behavior behind one verified primitive at a time.
4. Add an Acheron-style resolution/consequence layer after the verbs are stable.

## License

GPL-3.0.
