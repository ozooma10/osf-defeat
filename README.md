# OSF Defeat

OSF Defeat is a native **defeat framework** for Starfield, inspired by Skyrim's Acheron model and built on [CommonLibSF](https://github.com/Starfield-Reverse-Engineering/CommonLibSF).

When an actor would take a lethal Health hit, OSF Defeat clamps the damage before the engine commits death, puts the actor into a framework-owned defeated/down state, applies a recovery timer, persists the state through OSF Animation's `.osf` sidecar, and recovers the actor when the deadline is reached. The player path is the stable core; NPC defeat is supported through the Papyrus verbs and an opt-in auto-defeat for hostile NPCs.

## Features

- Lethal-damage clamp at the verified `ActorValueOwner::ModActorValue` hook site (`HookTrigger.md`), on the **Health** gauge.
- Player defeat: down/recovery state with combat suppression, JSON-selected consequence, and co-save recovery.
- NPC defeat: the live-validated per-actor down/recover recipe via the `DefeatActor`/`RecoverActor` verbs, plus opt-in auto-defeat for hostile NPCs (`npcAutoDefeat`).
- Aggressor attribution, including the script/null path via `currentCombatTarget`.
- 3D-reload re-assert (`Actor::Set3D`) so defeat state survives cell change / save load / `coc`.
- Robot/turret + hostility classification (`ClassifyActor`, `IsNonFlesh`, `IsHostile`) for NPC routing.
- **Defeat cause** (`GetDefeatCause` / `GetLastPlayerDefeatCause`): Health-down vs scripted,
  persisted — content can route a scripted/capture defeat differently from a beaten one.
- **Struggle primitives**: `OnPlayerDefeatImminent` fires below a configurable health fraction;
  `ResistDefeat` opens a no-defeat grace window (vanilla lethality) for QTE/struggle content.
- Defeat/recovery, AI-suspension, captive-faction, capture, and defeat-lease verbs backed by `DefeatRegistry` and OSF shared persistence.
- **Defeat lease + watchdog**: content acquires a held down-state under a stable holder ID with a deadline
  (`AcquireDefeatLease` / `ReleaseDefeatLease`, or `recoveryMode: "held"` in consequence JSON); a lapsed
  lease force-recovers and logs the defaulting consumer so broken content cannot strand the player.
- **Emergency escape / safe uninstall**: `cgf "OSFDefeat.ForceRecoverAll"` recovers resolvable
  actors, resumes framework-suspended AI, removes owned captive-faction memberships, and reports unresolved records to retry.
- **Legible death boundary**: every deliberate player passthrough (processing disabled, in-scene,
  resist window…) logs its reason; `explainDecisions` adds NPC passthroughs and full
  consequence-selection detail (per-consequence filter reasons + the priority/weight roll).
- Player defeated/recovered/consequence-started Papyrus events.
- Framework settings and player consequences under `Data/SFSE/OSFDefeat/` (schema v3 separates stable
  `id` from `displayName` and uses explicit recovery/lease terms; v1/v2 files remain valid).
- Central `Overrides.json`: users tune any installed consequence (enabled/weight/priority) by stable ID
  in one file, without editing pack files.
- `ReloadJSON` Papyrus helper to hot-reload settings/consequence/override JSON without a restart.
- **Basic Consequences pack** ([pack/](pack/README.md)): the in-box SFW default — "Robbed and
  Left" (credits to the aggressor, recoverable by hunting them down), shipped as a companion
  `OSFDefeatBasic.esm` and the reference consumer of the handoff API.

## Current Architecture

| Layer | Purpose |
|---|---|
| `Core/PlayerDefeat` | ModActorValue clamp (player + opt-in NPC), attribution, and player consequence orchestration. |
| `Core/Recovery` | Automatic recovery timers, recovery holds, defeat leases, and a direct post-Main-update watchdog tick every 500 ms. |
| `Core/Decision` | Robot/turret classification + hostility, for NPC routing and the auto-defeat gate. |
| `Core/Reattach` | `Actor::Set3D` hook that re-asserts down-state on 3D reload. |
| `Core/Consequence` | Loads and selects player consequence JSON. |
| `Core/Config` | Loads global framework settings, with legacy `Validation` directory support. |
| `Core/Eligibility` | Decides whether an actor pair may enter defeat. |
| `Core/DefeatRegistry` | Thread-safe active-defeat store. |
| `Core/Defeat` | Defeat/recovery and AI-suspension verbs. |
| `Core/Capture` | Persistent captor-to-captive relationships layered over defeat. |
| `Core/Persistence` | OSF Animation persistence client (`OSFD`), including automatic-recovery and lease deadlines. |
| `Core/RuntimeMutation` | Coordinates mutations with persistence snapshots, exposes the engine task queue, and invalidates work from a prior save. |
| `Core/Events` | Player defeated/recovered/consequence Papyrus event registration. |
| `Engine/Engine` | Starfield primitives for player and NPC down/recovery state. |
| `Papyrus/Functions` | Native bindings for `OSFDefeat.psc`. |

Worker/VM callbacks submit actor mutations through `RE::BSService::TaskQueue::AddTask`.
This is the engine's queue; Defeat has no command buffer or polling thread. The queue can execute
inline on its drain owner, so damage hooks submit defeat only **after the original health write
returns**, and the 3D hook submits re-assertion after the original attach. Early-boot submissions
are rejected while native queueing is disabled. Recovery runs directly after Main update.

## JSON

Default files ship in `dist/SFSE/OSFDefeat` and deploy to `SFSE/OSFDefeat`.
At runtime, settings, consequences, and overrides use `<Starfield.exe directory>/Data/SFSE/OSFDefeat`.
MO2 supplies installed mod content through that game's virtual `Data` directory.

Framework settings example (`Settings/default.json`, v3). The shipped file comments these keys out
so legacy user settings survive an upgrade; remove the `// ` prefix from keys you want to set:

```json
{
  "version": 3,
  "processingEnabled": true,
  "disableInScenes": false,
  "defaultAutoRecoveryDelaySeconds": 8.0,
  "npcAutoDefeat": false,
  "npcAutoDefeatIncludeRobots": false,
  "applyCaptiveFactionOnPlayerDefeat": false,
  "recentHostileAttackerRadius": 4096.0,
  "explainDecisions": false,
  "watchdogSeconds": 600.0,
  "excludedLocations": [],
  "excludedQuests": []
}
```

Player consequence example (v3):

```json
{
  "version": 3,
  "id": "example.down_then_recover",
  "displayName": "Down Then Recover",
  "priority": 0,
  "weight": 1,
  "autoRecoveryDelaySeconds": 8.0,
  "recoveryMode": "automatic",
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
`enabled`/`weight`/`priority` per consequence ID in one place.

`excludedLocations`, `excludedQuests`, and consequence `locations`/quest-state gates are reserved
schema fields. This CommonLibSF build does not yet expose proven safe engine seams for them, so
non-empty authored gates are warned and parked; they never call the known-unmapped location thunk.

## Papyrus API

The declarations live in `dist/Scripts/Source/OSFDefeat.psc`, with compiled output in `dist/Scripts/OSFDefeat.pex`.

Mutating `Bool` natives return request acceptance, not the operation's result. Work uses the native
engine queue and may execute inline on its drain owner. Confirm the eventual state with the
corresponding query/callback before a dependent action.

```papyrus
Bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
Bool Function RecoverActor(Actor akActor, Bool abResumeAI = True) global native
Bool Function ScheduleAutomaticRecovery(Actor akActor, Float afDelaySeconds) global native
Bool Function HoldRecovery(Actor akActor) global native
Float Function GetAutomaticRecoverySecondsRemaining(Actor akActor) global native
Bool Function SuspendActorAI(Actor akActor) global native
Bool Function ResumeActorAI(Actor akActor) global native
Bool Function AcquireDefeatLease(Actor akActor, String asHolderId, Float afTimeoutSeconds = 0.0) global native
Bool Function ReleaseDefeatLease(Actor akActor, String asHolderId) global native
String Function GetDefeatLeaseHolder(Actor akActor) global native
Bool Function ForceRecoverAll() global native
Int Function GetDefeatCause(Actor akActor) global native
Int Function GetLastPlayerDefeatCause() global native
Bool Function ResistDefeat(Actor akActor, Float afSeconds) global native
Bool Function IsResistingDefeat(Actor akActor) global native
Int Function RegisterForPlayerDefeatImminent(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerDefeatImminent(Int aiToken) global native
Bool Function IsDefeated(Actor akActor) global native
Bool Function IsActorAISuspended(Actor akActor) global native
Actor[] Function GetDefeated(Bool abLoadedOnly = False) global native
Int Function ClassifyActor(Actor akActor) global native ; 0 = flesh, 1 = robot, 2 = turret
Bool Function IsNonFlesh(Actor akActor) global native
Bool Function IsHostile(Actor akActor, Actor akTarget) global native
Function DisableProcessing(Bool abDisable) global native
Bool Function IsProcessingDisabled() global native
Int Function RegisterForPlayerDefeated(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerDefeated(Int aiToken) global native
Int Function RegisterForPlayerRecovered(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerRecovered(Int aiToken) global native
Int Function RegisterForPlayerConsequenceStarted(ScriptObject akReceiver, String asCallback) global native
Bool Function UnregisterForPlayerConsequenceStarted(Int aiToken) global native
Actor Function GetLastPlayerAggressor() global native
String Function GetLastPlayerConsequenceId() global native
String Function GetLastPlayerConsequenceQuest() global native
Float Function GetLastPlayerAutoRecoveryDelaySeconds() global native
Bool Function IsLastPlayerRecoveryHeld() global native
Actor[] Function GetLastPlayerRecentHostileAttackers() global native
Function ReloadJSON() global native
```

Registered receivers should implement `OnPlayerDefeated()`, `OnPlayerConsequenceStarted()`, and/or `OnPlayerRecovered()`. Consequence receivers should use the `GetLastPlayer*` functions to read the selected consequence context. Legacy rescue/pacify/claim/timer/context functions and v1/v2 JSON keys remain compatibility aliases; see [the vocabulary contract](docs/VOCABULARY.md) and `docs/papyrus-api.md`.

## Automated tests

```powershell
pwsh -NoProfile -File tools/test-smoke.ps1          # Native tests, payload checks, production build
pwsh -NoProfile -File tools/test-smoke.ps1 -RunGame # Also run a fresh local game smoke session
pwsh -NoProfile -File tools/test-smoke.ps1 -Plan    # Coverage and stage inventory
```

See [the testing guide](tests/README.md) for the nine native suites, isolated game setup,
JSON reports, missing-dependency preflight and remaining acceptance boundaries. Test builds
stay in private staging directories; game automation uses the sibling OSF Test Harness.
The live suite includes real save/load and sidecar validation, NPC capture/transfer/release,
Papyrus callbacks, input restoration and Basic Consequences effects. It privately builds
the sibling Animation dependency; carry its dynamic save-path fix when moving PCs.

## Build

```bat
xmake f -p windows -a x64 -m debug -y
xmake build
```

Set `XSE_SF_MODS_PATH` to your mod-manager `mods` folder, or `XSE_SF_GAME_PATH` to the game install, to auto-deploy the DLL/PDB, Papyrus files, and `SFSE/OSFDefeat` JSON. Set `OSF_DEFEAT_SKIP_DEPLOY=1` for a compile-only verification build.

To compile Papyrus manually:

```bat
"C:\Program Files (x86)\Steam\steamapps\common\Starfield\Tools\Papyrus Compiler\PapyrusCompiler.exe" OSFDefeat -i="dist\Scripts\Source;C:\Program Files (x86)\Steam\steamapps\common\Starfield\Data\Scripts\Source" -f="C:\Program Files (x86)\Steam\steamapps\common\Starfield\Data\Scripts\Source\Starfield_Papyrus_Flags.flg" -o="dist\Scripts"
```

## Limitations

Not yet supported: NPC consequence content (what happens to a defeated NPC beyond down/recover), follower-specific bleedout, Hunter Pride-style UI, authored teleport capture scenes, complex Acheron condition grammar, and interaction-menu options. Player consequence quest handoff is implemented. NPC auto-defeat is opt-in; its worker-thread classification reads are sandbox-confirmed safe, with broader in-game validation still recommended.

## License

GPL-3.0.
