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
- **Defeat cause** (`GetDefeatCause` / `GetLastPlayerDefeatCause`): Health-down vs EM-down vs
  scripted, persisted — content routes a stunned target differently from a beaten one.
- **Struggle primitives**: `OnPlayerDefeatImminent` fires below a configurable health fraction;
  `ResistDefeat` opens a no-defeat grace window (vanilla lethality) for QTE/struggle content.
- Defeat / Rescue / Pacify verbs backed by `DefeatRegistry` and SFSE co-save persistence.
- **Ownership lease + watchdog**: content claims a held down-state under its name with a deadline
  (`ClaimDefeat` / `ReleaseClaim`, or `holdRecovery` JSON auto-claims); a lapsed deadline
  force-rescues and logs the defaulting consumer — a broken content mod can never strand the player.
- **Emergency escape / safe uninstall**: `cgf "OSFDefeat.ForceRecoverAll"` rescues everyone,
  releases pacifies, strips captive factions, and empties the co-save.
- **Legible death boundary**: every deliberate player passthrough (processing disabled, in-scene,
  excluded location…) logs its reason; `explainDecisions` adds NPC passthroughs and full
  consequence-selection detail (per-outcome filter reasons + the priority/weight roll).
- Player defeated/recovered/consequence-started Papyrus events.
- JSON validation/config and player consequences under `Data/SFSE/OSFDefeat/` (schema v2:
  `enabled`, `cooldownSeconds`, `oncePerLocation`, `domain`, `maxHoldSeconds`; v1 files stay valid).
- Central `Overrides.json`: users tune any installed consequence (enabled/weight/priority) by name
  in one file, without editing pack files.
- `ReloadJSON` Papyrus helper to hot-reload validation/consequence/override JSON without a restart.
- **Basic Consequences pack** ([pack/](pack/README.md)): the in-box SFW default — "Robbed and
  Left" (credits to the aggressor, recoverable by hunting them down), shipped as a companion
  `OSFDefeatBasic.esm` and the reference consumer of the handoff API.

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

Validation example (v2):

```json
{
  "version": 2,
  "processingEnabled": true,
  "disableInScenes": false,
  "defaultRecoverySeconds": 8.0,
  "npcAutoDefeat": false,
  "npcAutoDefeatIncludeRobots": false,
  "explainDecisions": false,
  "watchdogSeconds": 600.0,
  "excludedLocations": [],
  "excludedQuests": []
}
```

Player consequence example (v2):

```json
{
  "version": 2,
  "name": "Alpha Down Then Recover",
  "priority": 0,
  "weight": 1,
  "recoverySeconds": 8.0,
  "enabled": true,
  "cooldownSeconds": 0.0,
  "oncePerLocation": false,
  "domain": "onfoot"
}
```

Selection filters first (disabled, off-domain, cooldown, once-per-location, conditions), then the
highest `priority` tier wins with weighted random among ties. A resolving `quest` token gets
`Quest.Start` dispatched before the consequence event fires. `"domain": "ship"` is schema-reserved
for the ship-defeat milestone. `Overrides.json` (root of `SFSE/OSFDefeat/`) lets users override
`enabled`/`weight`/`priority` per consequence name in one place.

## Papyrus API

The declarations live in `dist/Scripts/Source/OSFDefeat.psc`, with compiled output in `dist/Scripts/OSFDefeat.pex`.

```papyrus
Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RescueActor(Actor akVictim, Bool abUndoPacify = True) global native
Bool Function PacifyActor(Actor akActor) global native
Bool Function ReleaseActor(Actor akActor) global native
Bool Function ClaimDefeat(Actor akActor, String asOwner, Float afMaxHoldSeconds = 0.0) global native
Bool Function ReleaseClaim(Actor akActor, String asOwner) global native
String Function GetClaimOwner(Actor akActor) global native
Bool Function ForceRecoverAll() global native
Int Function GetDefeatCause(Actor akActor) global native
Int Function GetLastPlayerDefeatCause() global native
Bool Function ResistDefeat(Actor akActor, Float afSeconds) global native
Bool Function IsResistingDefeat(Actor akActor) global native
Int Function RegisterForPlayerDefeatImminent(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerDefeatImminent(Int aiToken) global native
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

Not yet supported: NPC consequence content (what happens to a defeated NPC beyond down/recover), follower-specific bleedout, Hunter Pride-style UI, teleport capture scenes, quest start/handoff, complex Acheron condition grammar, and interaction-menu options. NPC auto-defeat is opt-in; its worker-thread classification reads are sandbox-confirmed safe, with broader in-game validation still recommended.

## License

GPL-3.0.