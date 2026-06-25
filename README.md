# OSF Defeat

A native **defeat / death-alternative** framework for Starfield (a Skyrim "Acheron" analog),
built on [CommonLibSF](https://github.com/Starfield-Reverse-Engineering/CommonLibSF).

When a lethal hit would kill the player, they instead go **down**, nearby enemies **disengage**
for a short beat, and then the player is **revived** and gets back up — combat resumes.

```
lethal hit ─► player does NOT die, drops into BLEEDOUT
           ─► nearby enemies disengage (combat suppressed)
           ─► short "defeat beat" (~8s)
           ─► player REVIVED (gets back up), combat resumes
```

## How it works

Everything uses the engine's own machinery, so the plugin is **hook-free** — it only sets
engine flags / writes one engine global from a per-frame SFSE task:

| Step | Mechanism |
|---|---|
| **Don't die** | Sets the player base (`TESNPC`) **Essential** → the engine's own death path routes a lethal hit into bleedout. |
| **Enemies disengage** | Flips the global combat-AI gate that the `tcai` console command toggles (`DAT_1458d570d`). Per-actor `StopCombat` was tried and **fails** (the AI re-acquires the player next tick); the global gate suppresses target acquisition itself. |
| **Get back up** | The essential player has **no** natural bleedout recovery (it stays frozen at a negative-Health floor), so the player is actively revived with `RestoreActorValue(Health, max)`. |

Down-detection is pure Health polling — an essential player in bleedout reads `Health <= 0`
(live: ~ -30), which no living actor ever does. No damage hook required.

All addresses/offsets were reverse-engineered and live-validated in the OSF RE sandbox; see
its `gameplay.defeat_damage_hook` context-repo module for the full evidence trail.

## Requirements

* [SFSE](https://www.nexusmods.com/starfield/mods/106) (Starfield Script Extender)
* [Address Library for SFSE Plugins](https://www.nexusmods.com/starfield/mods/3256) — runtime
  AddrLib used to resolve the `tcai` handler (AddrLib ID **66826**)
* Build: [xmake](https://xmake.io) 3.0+ and an MSVC C++23 compiler

## Game version

Reverse-engineered against **Starfield 1.16.244**. The only version-specific anchor is the
`tcai` handler (AddrLib ID 66826); the combat-AI gate is parsed off that handler's flag-write,
so it relocates with the ID across builds. The `Actor+0x70` ActorValueOwner offset is also
1.16.244-specific. Re-verify both on a new game build.

## Build

```bat
xmake f -p windows -a x64 -m debug -y   :: first-time configure (MSVC)
xmake build
```

Set `XSE_SF_MODS_PATH` to your mod-manager `mods` folder (or `XSE_SF_GAME_PATH` to the game
install) and the build auto-deploys `OSF Defeat.dll` to `<that>\OSF Defeat\SFSE\Plugins\`.

## Install

Enable **OSF Defeat** in your mod manager (alongside SFSE + Address Library). No `.esp` is
needed — the player is made essential at runtime. Logs go to
`Documents\My Games\Starfield\SFSE\Logs\OSFDefeat.log`.

## Tuning

In [`src/Defeat.cpp`](src/Defeat.cpp):

* `kBeatMs` — how long the player stays down before the auto-revive (default 8000 ms).
* `RevivePlayer()` — currently heals to full; change the restore amount for a "get up
  wounded" feel.
* `kDownThreshold` / `kArmThreshold` — the bleedout-detection / re-arm Health thresholds.

## License

GPL-3.0.
