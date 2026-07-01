# OSF Defeat — Basic Consequences

The SFW default consequence pack that ships with OSF Defeat (roadmap M3), and the
**reference consumer** of the framework's handoff API: quest start, held recovery with the
auto-claim/watchdog lease, context getters (`GetLastPlayerAggressor`, `GetLastPlayerDefeatCause`),
and the explicit rescue handshake. It exists so the framework is worth installing on its own —
Acheron's empty-by-default mistake, not repeated.

Ships as its **own MO2 mod** (`OSF Defeat - Basic Consequences`) so users can toggle or replace
the default content without touching the framework.

## v0.1 — "Robbed and Left"

Defeated by human aggressors: they take a cut of your credits (default 10%, min 25, cap 2500)
and leave you for dead. The credits go **into the aggressor's inventory** — hunt them down to
get them back. After ~15s you come to. `cooldownSeconds: 300` keeps it from firing on every
skirmish; when nothing is eligible the framework's built-in down-then-recover fallback runs.

An optional `HurtAffliction` spell property (unfilled by default) casts a wound on the player as
they go down — fill it in xEdit/CK with a vanilla affliction spell to enable.

## Layout

| Path | What |
|---|---|
| `Plugin/` | Spriggit YAML source for `OSFDefeatBasic.esm` (one quest: `OSFDefeatBasicRobbedQuest`) |
| `Scripts/Source/OSFDefeatBasicRobbed.psc` | The consequence script |
| `SFSE/OSFDefeat/Consequences/Player/basic-robbed.json` | The consequence registration (v2 schema) |
| `Build.ps1` | Compile Papyrus → build ESM via Spriggit → deploy to MO2 |

## Build

```powershell
.\Build.ps1              # compile + esm + deploy
.\Build.ps1 -NoDeploy    # build only
```

## Planned (M3 remainder)

- Arrest path for security-faction aggressors (`Faction.SendPlayerToJail`).
- Filled vanilla affliction on the shipped record (needs verified spell FormIDs).
- Relocation ("wake up at the nearest civilized point") once placement validation is designed —
  the Shadow of Skyrim respawn-jank lesson says don't ship teleports without it.
- EM-cause variant outcomes once the condition grammar can gate on `defeatCause`.
