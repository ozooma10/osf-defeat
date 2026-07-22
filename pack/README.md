# OSF Defeat — Basic Consequences

The SFW default consequence pack that ships with OSF Defeat (roadmap M3), and the
**reference consumer** of the framework's handoff API: quest start, held recovery with the
auto-claim/watchdog lease, context getters (`GetLastPlayerAggressor`, `GetLastPlayerDefeatCause`),
and the explicit rescue handshake. It exists so the framework is worth installing on its own —
Acheron's empty-by-default mistake, not repeated.

Ships as its **own MO2 mod** (`OSF Defeat - Basic Consequences`) so users can toggle or replace
the default content without touching the framework.

## v0.3 — seven scenarios, one per defeat context

Each aggressor context produces a *plausible* beat instead of the framework's meaningless
built-in down-then-recover fallback. Selection is highest-priority-tier-wins, weights break ties
within a tier, and mutually-exclusive conditions (robot vs. human vs. creature) simply never
compete. The ladder:

| Priority | Scenario | Fires when | Beat |
|---|---|---|---|
| 20 | **Taken Into Custody** | security faction (UC/Freestar/Neon) | vanilla arrest via `Faction.SendPlayerToJail` — bounty, sentence, release all stock |
| 12 | **Left as a Message** | House Va'ruun zealots | fanatics don't loot: no credit loss, a vanilla concussion, short down |
| 10 | **Robbed and Left** | human aggressor (weight 3) | 10% credits into the aggressor's inventory; 35% chance of a minor sprain |
| 10 | **Picked Clean** | human aggressor (weight 1, long cooldown) | the harsher sibling: 25% credits **and your digipicks**, plus a contusion |
| 10 | **Left for Scrap** | robot / turret | no robbery motive: shortest down, an energy burn, quick recovery |
| 10 | **Mauled** | creature | longest down, deep lacerations, `oncePerLocation` so a den can't loop you |

**Recoverable loot.** Every credit/digipick theft goes **into the aggressor's inventory** (via
`RemoveItem`'s destination arg) — hunt the attacker down to get it back, never a flat deletion.

**Vanilla wounds.** Afflictions are added the vanilla way (`AddSpell`, they are ability spells —
*not* `Cast`), so they read as normal environmental injuries and any doctor or aid item clears
them. FormIDs mined from `Starfield.esm` with `tools/scan_esm.py` and filled onto the shipped
records; clear the `Wound`/`HurtAffliction` property in xEdit/CK to disable a wound.

When nothing is eligible the framework's built-in down-then-recover fallback runs.

## Layout

| Path | What |
|---|---|
| `Plugin/` | Spriggit YAML source for `OSFDefeatBasic.esm` (quests 000800–000806) |
| `Scripts/Source/*.psc` | The consequence scripts (one per scenario) |
| `SFSE/OSFDefeat/Consequences/Player/*.json` | The consequence registrations (v2 schema) |
| `tools/scan_esm.py` | Binary SPEL/FACT/KYWD/MISC FormID scanner (`OSF_SCAN_TYPES=KYWD python … <regex>`) |
| `Build.ps1` | Compile Papyrus → build ESM via Spriggit → deploy to MO2 |

## Build

```powershell
.\Build.ps1              # compile + esm + deploy
.\Build.ps1 -NoDeploy    # build only
```

## Planned (M3 remainder)

- Relocation ("wake up at the nearest civilized point") once placement validation is designed —
  the Shadow of Skyrim respawn-jank lesson says don't ship teleports without it.
- Richer item stripping for `Picked Clean` (a curated aid/ammo FormList) beyond credits + digipicks.
- Escalation for repeat defeats by the same faction (uses the cooldown/arbitration fields).
- Runtime verification that `ActorTypeCreature` / `ActorTypeRobot` fire on the intended aggressors
  (the negative case is proven; the positive keyword case is the open M0 test).
