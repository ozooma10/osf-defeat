# OSF Defeat — Basic Consequences

The SFW default consequence pack that ships with OSF Defeat (roadmap M3), and the
**reference consumer** of the framework's handoff API: quest start, held recovery with an
automatic watchdog-bounded defeat lease, context getters (`GetLastPlayerAggressor`, `GetLastPlayerDefeatCause`),
and the explicit recovery handshake. It exists so the framework is worth installing on its own —
Acheron's empty-by-default mistake, not repeated.

Ships as its **own MO2 mod** (`OSF Defeat - Basic Consequences`) so users can toggle or replace
the default content without touching the framework.

## v0.3 — six scenarios, one per defeat context

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

**Wake-up destination.** Every non-arrest quest (arrest already relocates you via
vanilla `SendPlayerToJail`) ends through the shared
`OSFDefeatBasicRecovery.FinishAftermath()` (`Scripts/Source/OSFDefeatBasicRecovery.psc`)
instead of calling `RecoverActor` directly. It looks for the nearest
`OSFDefeatBasicRecoveryMarker`-tagged XMarker in the player's *current cell* and
moves the player there before recovery; if none exists it falls back to
same-cell recovery (today's behavior, so this is a zero-regression seam) — a
generic defeat mod must never guess a distant destination and break a quest or
ship interior. The `OSFDefeatBasicRecoveryMarker` keyword (FormID `000807`) ships
in `OSFDefeatBasic.esm`; until a location patch places tagged markers, the
fallback path is what runs.

> **Spriggit authoring note.** An earlier attempt to add this keyword concluded
> that Spriggit 0.40.1 "silently drops brand-new record groups." That was wrong:
> the group folder was authored as `Keywords/<name>/RecordData.yaml`, copying the
> shape of `Quests/`. Only records with **children** (quests, cells) get a folder
> plus `RecordData.yaml`; a simple record is a **single flat file** named
> `<GroupName>/<EditorID> - <FormID6>_<ModKey>.yaml`. With the flat shape the
> pinned 0.40.1 builds the KYWD group fine and round-trips it losslessly. Ground
> truth for any new group: `spriggit serialize` an existing plugin that already
> contains that record type and copy the layout it emits.

## Layout

| Path | What |
|---|---|
| `Plugin/` | Spriggit YAML source for `OSFDefeatBasic.esm` (quests 000800–000806, keyword 000807) |
| `Scripts/Source/*.psc` | The consequence scripts (one per scenario) |
| `SFSE/OSFDefeat/Consequences/Player/*.json` | The consequence registrations (v3 schema) |
| `tools/scan_esm.py` | Binary SPEL/FACT/KYWD/MISC FormID scanner (`OSF_SCAN_TYPES=KYWD python … <regex>`) |
| `Build.ps1` | Compile Papyrus → build ESM via Spriggit → deploy to MO2 |

## Build

```powershell
.\Build.ps1              # compile + esm + deploy
.\Build.ps1 -NoDeploy    # build only
```

## Planned (M3 remainder)

- Runtime-verify the wake-up destination seam: with no tagged marker (fallback = today's
  same-cell recovery), then with a test XMarker carrying `OSFDefeatBasicRecoveryMarker`.
- Richer item stripping for `Picked Clean` (a curated aid/ammo FormList) beyond credits + digipicks.
- Escalation for repeat defeats by the same faction (uses the cooldown/arbitration fields).
- Runtime verification that `ActorTypeCreature` / `ActorTypeRobot` fire on the intended aggressors
  (the negative case is proven; the positive keyword case is the open M0 test).
