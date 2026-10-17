# OSF Defeat 0.1.0-alpha

First public alpha of OSF Defeat, a native defeat framework for Starfield. A hit that would
kill an actor puts them into a recoverable down-state instead. The framework then selects a
consequence, hands it to content, and guarantees recovery even when that content breaks.

This is an alpha for players who are comfortable reading an SFSE log and reporting what they
see. It is **PC only**: the framework is an SFSE plugin, so it cannot ship on Creations or Xbox.

## Requirements

| Requirement | Version |
| --- | --- |
| Starfield (Steam) | 1.16.244.0 |
| SFSE | build matching 1.16.244.0 |
| Address Library for SFSE | v21 (`versionlib-1-16-244-0.bin`) |
| OSF Animation | 1.5.0, including the dynamic save-path fix (`c94274d` or later) |

OSF Animation is required. It plays the down-state pose and stores Defeat's save data in its
`.osf` sidecar next to each save. Older OSF Animation builds write that sidecar to the default
save folder, so MO2 profile-local saves lose defeat state on load.

## Install

The release ships as two MO2 mods. Enable both, and enable `OSFDefeatBasic.esm` in the plugin list.

- **OSF Defeat**: the SFSE plugin, the `OSFDefeat` Papyrus script, default settings, and the
  built-in down-then-recover consequence.
- **OSF Defeat - Basic Consequences**: the optional default content pack, `OSFDefeatBasic.esm`
  and six consequences. Without it, every defeat is a plain eight-second down and recovery.

Settings live in `SFSE/OSFDefeat/Settings/`. Users can disable or reweight any consequence by
ID in `SFSE/OSFDefeat/Overrides.json` without editing pack files.

## What you get

**Player defeat.** A lethal Health hit puts you into a down-state instead of killing you.
Enemies disengage, you cannot be damaged while down, and combat resumes normally after you stand up.
Defeat state survives saving and loading, cell changes, and `coc`.

**Basic Consequences pack.** Each common aggressor gets a plausible outcome.

| Consequence | Aggressor | What happens |
| --- | --- | --- |
| Taken Into Custody | UC, Freestar or Neon security | Vanilla arrest and jail |
| Left as a Message | House Va'ruun | No theft, a concussion |
| Robbed and Left | Humans | Ten percent of your credits go to the attacker |
| Picked Clean | Humans, rarer | A quarter of your credits and your digipicks go to the attacker |
| Left for Scrap | Robots and turrets | Short down, an energy burn |
| Mauled | Creatures | Long down, lacerations, at most once per location |

Stolen items go into the aggressor's inventory, so you can hunt them down to get them back.
Wounds are vanilla afflictions that any doctor or aid item clears.

**Safety.** Content that holds you down must hold a lease with a deadline. If it never lets go,
a watchdog recovers you and names the mod that failed. If anything ever strands you, run:

```text
cgf "OSFDefeat.ForceRecoverAll"
```

**Transparency.** Every case where the framework deliberately lets you die is logged with its
reason. Set `explainDecisions` to `true` to also log why each consequence was or was not chosen.

**Content API.** Consequences are JSON files with priority, weight, cooldown, and actor
conditions. A consequence can start its own quest, hold recovery, read the aggressor and recent
attackers, and capture or transfer NPCs. See `docs/papyrus-api.md` and `docs/osf-surrender-port.md`.

## Uninstall

1. Load your save and run `cgf "OSFDefeat.ForceRecoverAll"`.
2. Save, quit, and disable both mods.

The force-recover call ends any active defeat and strips framework-applied factions, so the
next save carries no Defeat data.

## Verification

The automated bench passed on 2026-10-16 against this source. Its earlier game run covered
42 assertions in a quiet test cell, including real saves and loads, NPC capture, controls
after recovery, all Papyrus callbacks, and each pack consequence. A fresh offline rerun
passed 75 native cases / 889 assertions, runner/evidence checks, payload validation, and the
production build.

A separate production-build combat pass on the same day triggered all six consequences
from actual enemy attacks, including a Model A robot, Terrormorph, Va'ruun, and UC security.
It also verified save/load while down, emergency recovery, disabled-processing death/reload,
and opt-in NPC defeat/recovery from companion gunfire. Custody moved the player to the UC
security interior; full bounty and sentence accounting were not measured. See `TESTPLAN.md`
for retained evidence, setup, and scope. Testing used a private updated Animation DLL.

## Known limitations

- NPCs are not defeated by default. `npcAutoDefeat` is opt-in and has no NPC consequence content yet.
- Location and quest exclusion settings are accepted but ignored until their engine hooks are proven.
- Ship combat defeat is not implemented.
- Followers have no special down behavior.
- Robot and creature aggressor selection passed with real actors; defeat/capture presentation
  on nonhuman NPC skeletons is untested.
- Real jail sentences, controllers, cold restarts, and cross-cell captive transfers are untested.
- The combat pass used spawned hostiles and shortened player Health in a disposable save;
  extended natural-encounter playtesting and firearm controls after recovery remain open.
- Hooks are bound to Starfield 1.16.244.0. A game update can disable the plugin until it is rebuilt.

## Reporting problems

Include `Documents/My Games/Starfield/SFSE/Logs/OSF Defeat.log` and, if Papyrus logging is on,
the newest `Papyrus.0.log`. Turning on `explainDecisions` makes consequence reports much easier to diagnose.
