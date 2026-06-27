# OSF Defeat

OSF Defeat is a native **defeat framework** for Starfield, inspired by Skyrim's Acheron model and built on [CommonLibSF](https://github.com/Starfield-Reverse-Engineering/CommonLibSF).

When an actor would take a lethal Health (or EM) hit, OSF Defeat clamps the damage before the engine commits death, puts the actor into a framework-owned defeated/down state, applies a recovery timer, persists the state to the SFSE co-save, and recovers the actor when the deadline is reached. The player path is the stable core; NPC defeat is supported through the Papyrus verbs and an opt-in auto-defeat for hostile NPCs.

## Features

- Lethal-damage clamp at the verified `ActorValueOwner::ModActorValue` hook site (`HookTrigger.md`), covering both **Health and EM Health**.
- Player defeat: down/recovery state with combat suppression, JSON-selected consequence, and co-save recovery.
- NPC defeat: the live-validated per-actor down/recover recipe via the `DefeatActor`/`RescueActor` verbs, plus opt-in auto-defeat for hostile NPCs (`npcAutoDefeat`).
- Aggressor attribution, including the script/null path via `currentCombatTarget`.
- 3D-reload re-assert (`Actor::Set3D`) so defeat state survives cell change / save load / `coc`.
- Robot/turret + hostility classification (`ClassifyActor`, `IsNonFlesh`, `IsHostile`) for NPC routing.
- Defeat / Rescue / Pacify verbs backed by `DefeatRegistry` and SFSE co-save persistence.
- Player defeated/recovered/consequence-started Papyrus events.
- JSON validation/config and player consequences under `Data/SFSE/OSFDefeat/`.
- Debug Papyrus helpers for forcing defeat/recovery (player or by ref) and reloading JSON.

## Current Architecture

| Layer | Purpose |
|---|---|
| `Core/PlayerDefeat` | ModActorValue clamp (player + opt-in NPC), attribution, recovery tick. |
| `Core/Decision` | Robot/turret classification + hostility, for NPC routing and the auto-defeat gate. |
| `Core/Reattach` | `Actor::Set3D` hook that re-asserts down-state on 3D reload. |
| `Core/Consequence` | Loads and selects player consequence JSON. |
| `Core/Config` | Loads validation JSON and global settings. |
| `Core/DefeatRegistry` | Thread-safe actor defeat state store. |
| `Core/Defeat` | Acheron-style verbs: defeat, rescue, pacify, query. |
| `Core/Persistence` | SFSE co-save serialization, including `recoverAtDay`. |
| `Core/Events` | Player defeated/recovered/consequence Papyrus event registration. |
| `Engine/Engine` | Starfield primitives for player and NPC down/recovery state. |
| `Papyrus/Functions` | Native bindings for `OSFDefeat.psc`. |

## JSON

Default files ship in `dist/SFSE/OSFDefeat` and deploy to `SFSE/OSFDefeat`.

Validation example:

```json
{
  "version": 1,
  "processingEnabled": true,
  "disableInScenes": false,
  "defaultRecoverySeconds": 8.0,
  "npcAutoDefeat": false,
  "npcAutoDefeatIncludeRobots": false,
  "excludedLocations": [],
  "excludedQuests": []
}
```

Player consequence example:

```json
{
  "version": 1,
  "name": "Alpha Down Then Recover",
  "priority": 0,
  "weight": 1,
  "recoverySeconds": 8.0
}
```

Selection uses the highest `priority`, then weighted random among entries at that priority. `quest` is parsed and logged but not started in alpha.

## Papyrus API

The declarations live in `dist/Scripts/Source/OSFDefeat.psc`, with compiled output in `dist/Scripts/OSFDefeat.pex`.

```papyrus
Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native
Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native
Bool Function IsDefeated(Actor akActor) global native
Bool Function IsPacified(Actor akActor) global native
Actor[] Function GetDefeated(Bool abLoadedOnly = False) global native
Int Function ClassifyActor(Actor akActor) global native ; 0 = flesh, 1 = robot, 2 = turret
Bool Function IsNonFlesh(Actor akActor) global native
Bool Function IsHostile(Actor akActor, Actor akTarget) global native
Function DisableProcessing(Bool abDisable) global native
Bool Function IsProcessingDisabled() global native
Function RegisterForPlayerDefeated(Form akReceiver) global native
Function UnregisterForPlayerDefeated(Form akReceiver) global native
Function RegisterForPlayerRecovered(Form akReceiver) global native
Function UnregisterForPlayerRecovered(Form akReceiver) global native
Function RegisterForPlayerConsequenceStarted(Form akReceiver) global native
Function UnregisterForPlayerConsequenceStarted(Form akReceiver) global native
Actor Function GetLastPlayerAggressor() global native
String Function GetLastPlayerConsequenceName() global native
String Function GetLastPlayerConsequenceQuest() global native
Float Function GetLastPlayerRecoverySeconds() global native
Bool Function DebugDefeatPlayer() global native
Bool Function DebugRecoverPlayer() global native
Bool Function DebugDefeatRef(Int aiFormID) global native
Bool Function DebugRecoverRef(Int aiFormID) global native
Function ReloadJSON() global native
```

Registered receivers should implement `OnPlayerDefeated()`, `OnPlayerConsequenceStarted()`, and/or `OnPlayerRecovered()`. Consequence receivers should use the `GetLastPlayer*` functions to read the selected outcome context. See `docs/osf-surrender-port.md` for the OSF Surrender handoff pattern.

## Build

```bat
xmake f -p windows -a x64 -m debug -y
xmake build
```

Set `XSE_SF_MODS_PATH` to your mod-manager `mods` folder, or `XSE_SF_GAME_PATH` to the game install, to auto-deploy the DLL/PDB, Papyrus files, and `SFSE/OSFDefeat` JSON.

To compile Papyrus manually:

```bat
"C:\Program Files (x86)\Steam\steamapps\common\Starfield\Tools\Papyrus Compiler\PapyrusCompiler.exe" OSFDefeat -i="dist\Scripts\Source;C:\Program Files (x86)\Steam\steamapps\common\Starfield\Data\Scripts\Source" -f="C:\Program Files (x86)\Steam\steamapps\common\Starfield\Data\Scripts\Source\Starfield_Papyrus_Flags.flg" -o="dist\Scripts"
```

## Limitations

Not yet supported: NPC consequence content (what happens to a defeated NPC beyond down/recover), follower-specific bleedout, Hunter Pride-style UI, teleport capture scenes, quest start/handoff, complex Acheron condition grammar, and interaction-menu options. NPC auto-defeat is opt-in and still pending in-game validation (including a worker-thread read-safety check on the classification calls).

## License

GPL-3.0.