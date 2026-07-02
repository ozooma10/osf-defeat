# OSF Defeat — Basic Consequences

The SFW default consequence pack that ships with OSF Defeat (roadmap M3), and the
**reference consumer** of the framework's handoff API: quest start, held recovery with the
auto-claim/watchdog lease, context getters (`GetLastPlayerAggressor`, `GetLastPlayerDefeatCause`),
and the explicit rescue handshake. It exists so the framework is worth installing on its own —
Acheron's empty-by-default mistake, not repeated.

Ships as its **own MO2 mod** (`OSF Defeat - Basic Consequences`) so users can toggle or replace
the default content without touching the framework.

## v0.2 — "Robbed and Left" + "Taken Into Custody"

**Robbed and Left** (human aggressors, priority 10): they take a cut of your credits (default
10%, min 25, cap 2500) and leave you for dead. The credits go **into the aggressor's inventory**
— hunt them down to get them back. 35% chance the beating leaves a **vanilla minor sprain**
(`ENV_AFFL_Injury_Sprain_1`, applied the vanilla way via `AddSpell` — treatable at any doctor).
After ~15s you come to. `cooldownSeconds: 300` keeps it from firing on every skirmish.

**Taken Into Custody** (security aggressors, priority 20 — beats robbery): instead of dying to
UC/Freestar/Neon security, you're arrested. The script asks the aggressor for its own crime
faction (`Actor.GetCrimeFaction`) and runs the vanilla arrest (`Faction.SendPlayerToJail`,
inventory kept) — bounty, sentence, and release location are all stock behavior. Gated on the
security-faction FormIDs mined from Starfield.esm (see `basic-arrested.json`).

When nothing is eligible the framework's built-in down-then-recover fallback runs.

## Layout

| Path | What |
|---|---|
| `Plugin/` | Spriggit YAML source for `OSFDefeatBasic.esm` (quests 000800 robbed, 000801 arrested) |
| `Scripts/Source/*.psc` | The consequence scripts |
| `SFSE/OSFDefeat/Consequences/Player/*.json` | The consequence registrations (v2 schema) |
| `Build.ps1` | Compile Papyrus → build ESM via Spriggit → deploy to MO2 |

## Build

```powershell
.\Build.ps1              # compile + esm + deploy
.\Build.ps1 -NoDeploy    # build only
```

## Planned (M3 remainder)

- Relocation ("wake up at the nearest civilized point") once placement validation is designed —
  the Shadow of Skyrim respawn-jank lesson says don't ship teleports without it.
- EM-cause variant beats (the condition grammar now supports `"cause": ["em"]`; needs an authored
  outcome worth the gate, e.g. stunned-and-captured).
- Escalation for repeat defeats by the same faction (uses the cooldown/arbitration fields).
