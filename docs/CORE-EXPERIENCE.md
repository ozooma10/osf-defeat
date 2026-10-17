# OSF Defeat — Core Experience: Shadows of Starfield

Status: proposal, 2026-10-16. Decides what the shipped content is, so that OSF Defeat is worth
installing on its own. The framework stays the platform; this is the flagship content built on
it, and the reference for how other authors extend it.

## 1. Why the current build doesn't land

Defeat today is a pause, not a transition. You cower for 8–30 seconds, a toast says some
credits moved, and you stand up where you fell, in front of the same enemies, who shoot you
again. The Robbed quest's own comment says the fight "resumes on vanilla terms." That is worse
than dying and reloading. Nothing in the world changed, no story started, and there is nothing
to go back for. Six scenarios that differ only in toast text do not fix that.

## 2. The reference: Shadow of Skyrim

[Shadow of Skyrim](https://www.nexusmods.com/skyrimspecialedition/mods/65136) (SoloManGames,
SKSE, ~735k unique downloads) is the proof that this loop works in a Bethesda game. Its pillars:

| Pillar | What it does in Skyrim |
|---|---|
| **Nemesis** | The enemy who defeats you gets a unique title, stat boosts, and a themed buff. A Stormcloak may gain a bear companion. Up to 5 nemeses at once. |
| **Gear theft** | The nemesis may take your gold and wear or wield your gear if it is better than theirs. |
| **Player debuff** | You get a debuff chosen by how you went down: a power attack gives "Off-Balance." |
| **Reward buff** | Killing your nemesis removes the debuff and grants a weaker version of their buff. Only if they are at or above your level. |
| **Alternative death** | You wake somewhere situational: jail if a guard beat you, a vampire lair if a vampire did. Nearby is preferred (same hold, then parent location, then landmass, then last bed or random). |
| **Two quests** | Retrieve your dropped pack at the defeat site, and hunt your nemesis. Objectives use generated directions such as "Return to the Shrine of Mara, near Morthal." |
| **Follower revive** | Followers get a limited window to clear the fight and revive you with no penalty. If they fail, you are defeated normally. |
| **Continuous play** | No reload. If you fell somewhere you cannot re-enter, it autosaves and lets you die normally. |
| **Guards** | Quest-related, essential, and summoned actors never become nemeses. Enemies far above your level do not either. When nemesis slots are full, no new nemesis is made, so you cannot shed debuffs by dying. |

What it deliberately does **not** do, by its author's own list: no dialogue tracking, no
nemesis-to-nemesis interaction, no hierarchies or power levels, no forts, no showdowns, no
nemesis escapes or re-encounters, no nemesis followers, no vendettas. Warner Bros. holds a
patent on the Shadow of Mordor Nemesis system, and that list is Shadow of Skyrim staying clear
of it. **Shadows of Starfield stays inside the same envelope.** That rules out the earlier idea
of a ringleader who taunts you on return. This is a design boundary, not legal advice.

## 3. Why Starfield is a better fit than Skyrim, and where it is harder

Better:

- **The player's ship is a natural wake-up point.** It is usually landed on the same planet,
  it is pressurized, and companions live there. Skyrim had to invent respawn spots.
- **Afflictions are a native, visible status system.** Debuffs read as part of the game.
- **Cities have clinics.** "Someone dropped you at Reliant Medical; here is the bill" is a
  wake-up that costs credits without taking anything you have to hunt for.
- **Factions are strongly themed.** Crimson Fleet, Ecliptic, Spacers, Va'ruun, robots, and
  Terrormorphs each suggest obvious titles and buffs.
- **The framework already exists.** Death prevention, the held down-state, selection, the
  handoff, custody, captive-faction pacification, and recovery safety are done and tested.

Harder:

- **Procedurally generated sites can vanish.** Surface points of interest are generated per
  landing. A nemesis or dropped pack left in one may not survive leaving the planet. This is
  the biggest technical risk and gets a spike before anything else (section 8, slice 0).
- **Hostile environments.** Taking a spacesuit on a toxic planet can turn a defeat into a
  death loop. The default never takes suit, helmet, or boost pack.
- **Spawned enemies are not persistent.** A nemesis must be held in a quest alias, and that
  hold must be proven to survive cell cleanup.
- **Names are Message records.** `SetOverrideName` takes a `Message`, not a string. Until a
  native name seam exists, titles come from a pre-authored pool.
- **Space and boarding.** Defeat aboard a ship in orbit has no safe wake-up yet. It passes
  through to vanilla death, the same as Shadow of Skyrim's "cannot re-enter" rule.

## 4. The design

### 4.1 One defeat, end to end

> On Jemison, an Ecliptic merc with a shotgun closes the distance and you drop. Sarah is still
> shooting. She has twenty seconds to finish it. She doesn't.
>
> Six hours later you wake in your ship's bunk. Sarah says she dragged you back. Your Coachman
> is gone, and your status shows **Shell-Shocked**: weapon sway after you take damage. Your
> journal has two new entries. *Retrieve your pack: the ridge above the abandoned mine, west
> of your landing site.* And *Hunt: Dax Vey, "Close Quarters," Ecliptic.*
>
> When you find him, he is carrying your Coachman, and he hits harder than he did. You win.
> The sway is gone, and you have a small close-range damage bonus called **Close Quarters**.

That is the whole product. Every system below exists to produce that story with any enemy,
anywhere, without authoring it per encounter.

### 4.2 The loop

```
 DOWN → RESCUE WINDOW → BLACKOUT → WAKE UP → THE HUNT
        (companion       (fade,      (situational  (pack quest +
         may revive)      hours)      place)        nemesis quest)
```

1. **Down.** The framework catches the lethal hit and holds the player down, as it does now.
2. **Rescue window.** If a companion is alive nearby, they get a limited time (default 20 s)
   to clear the hostiles. If they succeed, they revive you with partial health and there is no
   penalty. This is buildable on the current API: held recovery plus the recent-hostile list.
3. **Blackout.** Fade out. The aggressor is promoted to nemesis, takes what they take, and the
   player gets a debuff. `Game.PassTime` advances 2–8 hours (Shadow of Skyrim capped its
   maximum at 12 for needs-mod users).
4. **Wake up.** The player is placed at the best available wake-up site (4.4). A short
   aftermath message says who, where, and what is missing.
5. **The hunt.** Two quest objectives: the dropped pack at the defeat site, and the nemesis.

### 4.3 Nemesis

- **Eligibility.** Not essential, not protected, not quest-related, not summoned, not more
  than a configurable number of levels above the player (default 15; Starfield levels run
  higher than Skyrim's). No free slot means no new nemesis, and the rest of the loop still runs.
- **Slots.** Five, each a reference alias on one nemesis quest. The alias makes them
  persistent and carries their name and map marker.
- **Title.** Drawn from a pool for their archetype: human faction, robot, or creature. Pool
  entries are pre-authored `Message` records such as "Dax Vey, 'Close Quarters'". Creatures
  get titles only ("the Ridge Terror").
- **Boost.** Health and damage scaled by tier, plus `SetScale` growth for creatures.
- **Buff.** One themed ability. Examples, all illustrative:

| Archetype | Buff | Effect |
|---|---|---|
| Crimson Fleet | Boarding Captain | arrives with two pirate allies |
| Ecliptic | Close Quarters | bonus damage within 10 m |
| Spacer | Scavver | uses your stolen gear and heals on kill |
| Va'ruun | Serpent's Zeal | melee damage and stagger resistance |
| Robot | Overclocked | speed and energy resistance |
| Creature | Apex | size, health, and health regeneration |

- **Gear.** Takes your equipped weapon if it is better than theirs and equips it, plus a cut of
  your credits. Everything they carry drops when they die, so recovery is just the normal loot.

### 4.4 Wake-up sites, in priority order

Shadow of Skyrim prefers same hold, then parent location, then landmass. The Starfield analog
is same location, then same planet, then same system. Never guess a distant destination.

| Priority | Situation | Wake-up site | Cost |
|---|---|---|---|
| 1 | Security aggressor | Jail, via the existing custody path | vanilla bounty |
| 2 | Pirate or mercenary captor, no companion | Their holding cell, an authored interior; exit returns you to the defeat site | gear in a locked chest |
| 3 | Defeated in or near a city | That city's clinic | medical bill in credits |
| 4 | Your ship is landed on this planet | Your ship, or beside it if the interior move is unproven | none extra |
| 5 | Owned outpost on this planet | The outpost | none extra |
| 6 | Fallback | The defeat site, hours later, enemies gone, post-recovery truce | none extra |
| — | In space, aboard a hostile ship, or a flagged quest location | Vanilla death (passthrough) | reload |

Each site type is a keyword on an XMarker. The pack ships markers for the major cities and the
holding cell; other mods add more by placing tagged markers. This replaces the current
same-cell-only recovery-marker seam with the same idea at planet scale.

### 4.5 Traits: debuffs and rewards

- **Debuff** picked by how you went down. Weapon category comes from the aggressor's
  equipped weapon through Papyrus today. Damage type would come from a framework seam later.

| Went down to | Debuff |
|---|---|
| Shotgun or melee at close range | Shell-Shocked: weapon sway after taking damage |
| Rifle or sniper | Rattled: aim sway while scoped |
| Energy weapon | Scorched: lower energy resistance |
| Explosive | Ringing Ears: lower max health |
| Robot | Compromised Seal: lower environmental protection |
| Creature | Infected Wound: no health regeneration |

- Debuffs are ability spells, not vanilla afflictions, so doctors and aid items do **not**
  cure them. Only killing the nemesis does. If someone else kills them, the debuff clears with
  no reward.
- **Reward** is a weaker version of the nemesis buff, granted only for nemeses at or above
  your level.
- **Five trait slots**, shared between debuffs and rewards. When full, the oldest is replaced.

### 4.6 Gear retrieval

The rest of what was taken goes into a dropped pack, a container placed at the defeat site
with its own objective. The default takes only weapons and a credit cut, so the pack holds the
non-equipped weapons. A "full strip" setting matches Shadow of Skyrim and puts everything but
the suit in the pack. Directions use location aliases: "Return to the ridge above
<location>, <planet>."

## 5. Extensibility: how other authors add content

The pack ships its libraries as `FormList`s and keyworded markers, so other plugins extend it
without touching our records:

- **Titles**: add `Message` records to an archetype's title list.
- **Buffs, debuffs, rewards**: add spell triples to the trait lists, with conditions.
- **Wake-up sites**: place an XMarker with a site keyword in any cell.
- **Replace the whole loop**: ship a consequence JSON at higher priority. The framework's
  selection and overrides already handle this.

User tuning goes through OSF Settings: nemesis count, level cap, time skip, strip mode,
rescue window, and which wake-up tiers are enabled.

## 6. Framework vs. pack

**Already possible with the current framework:** the nemesis quest and aliases, titles,
buffs, debuffs, gear transfer, the rescue window, `PassTime`, relocation, and quest objectives.
Most of Shadows of Starfield is Papyrus content on the existing API.

**New framework seams, each general enough for other authors:**

| Seam | Why |
|---|---|
| **Post-recovery truce** | The fallback wake-in-place needs nearby hostiles to stay non-hostile until the player attacks or a timer expires. The captive faction is framework-owned and removed on recovery today. |
| **Quest-related actor check** | Eligibility. Papyrus cannot tell whether an actor is in another quest's alias. A native check can. |
| **Defeat site record** | Position, cell, location, and planet of the defeat, persisted, with a Papyrus getter. Every pack needs it. |
| **Killing-blow context** | Damage type and weapon at the clamp, for situational debuffs beyond weapon category. Only what the hook can prove. |
| **Native name override** (later, RE) | Generated names instead of a pre-authored pool. |
| **Down-beat scene hook** (later) | Let a consequence name its own OSF Animation scene instead of the bare cower. |

## 7. What happens to Basic Consequences

Shadows of Starfield replaces it as the in-box pack. Custody becomes wake-up tier 1. Robbery
becomes the nemesis gear and credit take. Left for Scrap and Mauled become the robot and
creature archetypes. Left as a Message becomes Va'ruun titles and buffs. Picked Clean goes.

Parked until the on-foot loop is good: ship-domain defeat (roadmap M5), the surrender prompt,
and NPC consequence content.

## 8. Build order

Each slice is playable and testable on its own.

**Implementation status, 2026-10-16:** slice 0 now has a separate
[persistence probe and step-by-step fixture guide](../tests/persistence-spike/README.md).
Its Papyrus compiles, the test ESM builds and round-trips, and the isolated-profile
preparation checks pass. The [corrected reruns](../tests/persistence-spike/RESULTS-2026-10-16-corrected.md)
pass cold restart, two normal planet round trips with away-save reloads, a 48-local-hour
wait, alias names/journal substitution, and movement to a captured landed-ship interior
with working controls and exit before/after reload. Identity uses per-instance stamps;
departure observes alias detach events, and return compares live reference cells.
**Slice 0's feasibility gate passes for the two tested sites. Minimal Shadows is implemented as a staged pack.**
The generated seed is an outdoor natural feature, and the actor is an inert surrogate.
Existing-aggressor promotion at an enemy structure, actual cell reset, and weapon-instance
transfer still need acceptance coverage. The authored surrogate intermittently rendered
only partially despite passing identity/loaded checks; include appearance in that coverage.
The [slice-1 implementation and acceptance record](MINIMAL-SHADOWS-IMPLEMENTATION.md)
covers the one-slot hunt, traits, blackout, theft, recovery truce and death cleanup.
Its focused disposable-cell suite passes all 15 assertions. Basic Consequences remains
the shipped content pending the broader acceptance gates recorded there.

0. **Spike: persistence.** Prove that a quest-aliased actor and a placed container survive
   leaving and returning to a planet, in both an authored location and a procedurally
   generated one. Prove `MoveTo` into the landed ship's interior. Prove alias display names.
   The answers decide whether the pack and nemesis stay at the defeat site or need another plan.
1. **Minimal Shadows.** Aggressor becomes nemesis with a title and one buff, takes your weapon
   and credits, you get a debuff, you wake at the defeat site hours later with a truce, and a
   hunt objective points at them. Killing them clears the debuff and grants the reward. This
   slice alone fixes "I get shot at again when I recover."
2. **Wake-up table.** Ship, city clinic with a bill, jail, then fallback.
3. **Gear retrieval.** Dropped pack at the defeat site with directions.
4. **Companion rescue window.**
5. **Library breadth.** Titles, buffs, and debuffs for each archetype, and situational debuffs.
6. **Extensibility and settings.** FormList injection documented, OSF Settings page.
7. **Holding cell and presentation.** The captor interior, a wake-up animation, an aftermath
   card through OSF UI.

## 9. Open questions

- **Name.** "Shadows of Starfield" signals the inspiration clearly. Whether to use it
  publicly, given the Warner Bros. patent and Shadow of Skyrim's care around it, is your call.
- **Persistence in generated sites.** Slice 0 passed for alias-held references at an outdoor
  natural feature across normal planet travel and a 48-local-hour wait. Enemy structures,
  existing aggressors, and actual cell reset remain unproven. If those locations are lost,
  the hunt must happen before departure or use a persistent location without becoming an
  "escape and re-encounter."
- **Space and ship interiors.** Passthrough to vanilla death is the safe start. Waking aboard
  your own docked ship after a boarding fight is a possible later tier.
- **Time skip.** 2 hours is safe for quest timers. 6–8 sells the blackout better.
- **Nemesis lifetime.** Five slots bound it. Decide whether a nemesis you ignore for many game
  days expires and frees the slot without a reward.
