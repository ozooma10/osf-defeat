# OSF Defeat Alpha Roadmap

## Alpha Goal

Build OSF Defeat as a playable, player-first “Acheron for Starfield” slice: native lethal-damage interception for the player, a real defeated/down state, JSON-authored config/consequences, co-save recovery, and a small Papyrus surface for content authors.

## Implemented Alpha Slice

- Player-only lethal health damage hook at the verified `ActorValueOwner::ModActorValue` site.
- Defeat registry, persistence, and Defeat / Rescue / Pacify verbs retained as the foundation spine.
- Player down/recovery engine state with combat AI suppression during defeat.
- `recoverAtDay` stored in co-save so save/load during defeat can resume recovery.
- JSON validation under `Data/SFSE/OSFDefeat/Validation/*.json`.
- JSON player consequences under `Data/SFSE/OSFDefeat/Consequences/Player/*.json`.
- Highest-priority, weighted-random player outcome selection with built-in fallback.
- Player defeated/recovered Papyrus event registration.
- Debug Papyrus helpers: `DebugDefeatPlayer`, `DebugRecoverPlayer`, `ReloadJSON`.
- Package defaults in `dist/SFSE/OSFDefeat` plus Papyrus source/PEX in `dist/Scripts`.

## JSON Schemas

Validation v1:

```json
{
  "version": 1,
  "processingEnabled": true,
  "disableInScenes": false,
  "defaultRecoverySeconds": 8.0,
  "excludedLocations": [],
  "excludedQuests": []
}
```

Player consequence v1:

```json
{
  "version": 1,
  "name": "Alpha Down Then Recover",
  "priority": 0,
  "weight": 1,
  "recoverySeconds": 8.0,
  "quest": "OptionalQuestEditorID"
}
```

`quest` is parsed and logged only in alpha; quest start/handoff is post-alpha.

## Alpha Test Plan

- Lethal player hit causes defeat instead of death.
- Player recovers after configured `recoverySeconds`.
- Combat suppression/recovery does not permanently break AI.
- Save/load while defeated preserves and resumes defeat state.
- Invalid JSON logs an error and falls back safely.
- `DisableProcessing(true)` lets vanilla death/behavior proceed without OSF handling.
- `xmake build` passes and deploys DLL/PDB, Papyrus files, and JSON defaults.

## Post-Alpha Milestones

1. Runtime-test and harden the player hook across Starfield updates.
2. Add quest start/handoff for configured player consequences.
3. Expand validation from simple exclusions to a real condition grammar.
4. Add interaction/menu registration for content mods.
5. Revisit NPC/follower defeat only after the player path is stable.

## Explicitly Unsupported In Alpha

NPC defeat, follower defeat, Hunter Pride-style UI, teleport capture scenes, full content event packs, complex Acheron condition grammar, and automatic quest/capture scene startup.