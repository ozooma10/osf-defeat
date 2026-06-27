# OSF Defeat

OSF Defeat is a native **player-first defeat framework alpha** for Starfield, inspired by Skyrim's Acheron model and built on [CommonLibSF](https://github.com/Starfield-Reverse-Engineering/CommonLibSF).

The alpha slice is intentionally narrow: when the player would take lethal health damage, OSF Defeat intercepts the damage, puts the player into a defeated/down state, applies a JSON-selected recovery timer, persists the state to the SFSE co-save, and recovers the player when the deadline is reached.

## Alpha Features

- Player-only lethal damage interception through the verified `ActorValueOwner::ModActorValue` hook site documented in `HookTrigger.md`.
- Defeat / Rescue / Pacify verbs backed by `DefeatRegistry` and SFSE co-save persistence.
- Player defeated/recovered Papyrus events.
- JSON validation/config under `Data/SFSE/OSFDefeat/Validation/*.json`.
- JSON player consequences under `Data/SFSE/OSFDefeat/Consequences/Player/*.json`.
- Built-in fallback outcome: down, suppress combat briefly, recover.
- Debug Papyrus helpers for forcing player defeat/recovery and reloading JSON.

## Current Architecture

| Layer | Purpose |
|---|---|
| `Core/PlayerDefeat` | Player lethal-damage hook, outcome selection, recovery deadline/tick. |
| `Core/Consequence` | Loads and selects player consequence JSON. |
| `Core/Config` | Loads validation JSON and global alpha settings. |
| `Core/DefeatRegistry` | Thread-safe actor defeat state store. |
| `Core/Defeat` | Acheron-style verbs: defeat, rescue, pacify, query. |
| `Core/Persistence` | SFSE co-save serialization, including `recoverAtDay`. |
| `Core/Events` | Player defeated/recovered Papyrus event registration. |
| `Engine/Engine` | Small Starfield primitives for player down/recovery state. |
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
Function DisableProcessing(Bool abDisable) global native
Bool Function IsProcessingDisabled() global native
Function RegisterForPlayerDefeated(Form akReceiver) global native
Function UnregisterForPlayerDefeated(Form akReceiver) global native
Function RegisterForPlayerRecovered(Form akReceiver) global native
Function UnregisterForPlayerRecovered(Form akReceiver) global native
Bool Function DebugDefeatPlayer() global native
Bool Function DebugRecoverPlayer() global native
Function ReloadJSON() global native
```

Registered receivers should implement `OnPlayerDefeated()` and/or `OnPlayerRecovered()`.

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

## Alpha Limitations

Unsupported for alpha: NPC defeat, follower defeat, Hunter Pride-style UI, teleport capture scenes, quest start/handoff, complex Acheron condition grammar, and interaction-menu options.

## License

GPL-3.0.