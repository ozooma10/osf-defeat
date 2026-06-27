# OSF Defeat Alpha Roadmap

## Alpha Goal

Build OSF Defeat as a playable, player-first “Acheron for Starfield” slice: native lethal-damage interception for the player, a real defeated/down state, JSON-authored config/consequences, co-save recovery, and a small Papyrus surface for content authors.

## Implemented Slice

- Player lethal-damage clamp at the verified `ActorValueOwner::ModActorValue` site, covering
  **both Health and EM Health** (a lethal EM hit routes into defeat instead of the vanilla shock).
- Aggressor attribution on the script/null path via the victim's `currentCombatTarget`.
- `Actor::Set3D` re-assert hook so defeat state survives cell change / save load / `coc`.
- Defeat registry, persistence, and Defeat / Rescue / Pacify verbs as the foundation spine.
- Player down/recovery engine state with combat AI suppression during defeat.
- **Verb-driven NPC defeat/recover** using the live-validated per-actor recipe (bleedout + ghost +
  per-actor combat removal). Auto-defeat on NPC lethal hits is intentionally deferred (see below).
- Recovery generalized to any defeated actor by `recoverAtDay`; held entries await explicit rescue.
- `recoverAtDay` stored in co-save so save/load during defeat can resume recovery.
- JSON validation under `Data/SFSE/OSFDefeat/Validation/*.json`.
- JSON player consequences under `Data/SFSE/OSFDefeat/Consequences/Player/*.json`.
- Highest-priority, weighted-random player outcome selection with built-in fallback.
- Player defeated/recovered Papyrus event registration.
- Player consequence handoff context and `OnPlayerConsequenceStarted` registration for content mods.
- Debug Papyrus helpers: `DebugDefeatPlayer`, `DebugRecoverPlayer`, `DebugDefeatRef`,
  `DebugRecoverRef`, `ReloadJSON`.
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
2. Add direct quest start/fill behavior for configured player consequences. The current alpha exposes a registered handoff event plus queryable context.
3. Expand validation from simple exclusions to a real condition grammar.
4. Add interaction/menu registration for content mods.
5. NPC defeat: add the faction/robot hostility discriminator and an NPC consequence policy, then
   enable auto-defeat on NPC lethal hits (generalize the clamp past the player-only gate). The
   per-actor down/recover recipe and verbs are already in place.

## Explicitly Unsupported (yet)

Automatic NPC defeat on every lethal hit (needs the faction/robot gate above), follower-specific
bleedout behavior, Hunter Pride-style UI, teleport capture scenes, full content event packs, complex
Acheron condition grammar, and automatic quest/capture scene startup.