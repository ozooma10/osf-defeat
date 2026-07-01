# OSF Defeat Roadmap

## Vision

OSF Defeat started as "Acheron for Starfield." The reframe: it is a **defeat framework judged by
whether content authors choose it and users trust it**, not by an Acheron feature checklist.

What we keep from Acheron, because it is proven:

- **Native hook interception, no polling** — the `ModActorValue` clamp (Health + EM) instead of
  `OnHit` cloaks or Papyrus polling.
- **Framework owns physics, content owns meaning** — defeat state, pacify, validation, selection,
  handoff live here; scenes, robbery, capture, outcomes live in content mods.
- **Declarative drop-in registration** — JSON consequence files; priority → conditions → weighted
  random selection.
- **Design humility about death** — scripted kills and excluded contexts pass through; the
  framework chooses when to save you, and never soft-locks a quest to do it.

What we deliberately do better than Acheron (its known adoption problems):

- **Broken consumers must not strand the player** → ownership lease + watchdog (M1).
- **Opaque decisions** ("why didn't defeat fire?") → decision transparency / explain mode (M1).
- **Coarse cross-mod arbitration** (weights only) → cooldowns, once-per-location, central
  user overrides (M2).
- **Empty out of the box** → a shipped, vanilla-integrated default consequence pack (M3).

What we exploit that Skyrim never had:

- **EM damage as a first-class non-lethal channel** → stun-and-capture NPC defeat identity (M4).
- **Ship combat as a second defeat domain** → "ship disabled → boarded" instead of exploding (M5).
- **Vanilla consequence primitives** — afflictions, jail (`SendPlayerToJail`), bounty/faction rep —
  as default content that feels native (M3).

Stated position: OSF Defeat is SFSE-native and therefore **PC-only**. The native hook is the whole
reason the core is solid; we accept the no-Creations/Xbox trade explicitly (README docs task, M0).

## Implemented Slice

- Player lethal-damage clamp at the verified `ActorValueOwner::ModActorValue` site, covering
  **both Health and EM Health** (a lethal EM hit routes into defeat instead of the vanilla shock).
- Aggressor attribution on the script/null path via the victim's `currentCombatTarget`.
- `Actor::Set3D` re-assert hook so defeat state survives cell change / save load / `coc`.
- Defeat registry, persistence, and Defeat / Rescue / Pacify verbs as the foundation spine.
- Player down/recovery engine state; combat is deliberately **not** paused during the down beat
  (player is ghosted; enemies resume naturally on recovery — no tcai-gate freeze).
- **Verb-driven NPC defeat/recover** using the live-validated per-actor recipe (bleedout + ghost +
  per-actor combat removal).
- Recovery generalized to any defeated actor by `recoverAtDay`; held entries await explicit rescue.
- `recoverAtDay` stored in co-save so save/load during defeat can resume recovery.
- JSON validation under `Data/SFSE/OSFDefeat/Validation/*.json`; player consequences under
  `Data/SFSE/OSFDefeat/Consequences/Player/*.json`.
- Highest-priority, weighted-random player outcome selection with built-in fallback.
- Player defeated/recovered/consequence-started Papyrus events with handoff context getters.
- Robot/turret + hostility classification (`ClassifyActor`, `IsNonFlesh`, `IsHostile`).
- Opt-in **NPC auto-defeat** (`npcAutoDefeat`) on would-be-lethal hits, symmetric with the player path.
- `ReloadJSON` hot-reload of validation/consequence JSON without a restart.
- **SAF-parity handoff API**: consequence `quest` tokens VM-dispatch `Quest.Start`
  (`startQuest` opt-out); `SetRecoveryTimer` / `GetRecoverySecondsRemaining` + per-consequence
  `holdRecovery`; captive-faction pacify verbs (`ApplyCaptiveFaction` / `RemoveCaptiveFaction` /
  `IsCaptiveFactionApplied`), co-save persisted, auto-removed on rescue; defeat-time participant
  snapshot (`GetLastPlayerParticipants`, `participantScanRadius`).
- **Condition grammar (actor tier)**: per-consequence aggressor/victim keyword+faction matching
  (any/all semantics, Acheron-style) as a selection gate before the priority/weight roll.
- Package defaults in `dist/SFSE/OSFDefeat` plus Papyrus source/PEX in `dist/Scripts`.

## Milestone 0 — Alpha Hardening (now)

Ship a build a user can trust for a whole playthrough.

- [ ] In-game verify the current no-pacify build: down → recover with no `tcai`, no enemy freeze,
      no crash (deployed but not yet re-confirmed).
- [ ] Runtime-verify the robot/turret keyword **positive** case (negative proven across 3800+ reads).
- [ ] Alpha test plan (below) green end-to-end.
- [ ] Hook resilience across Starfield updates (Address Library discipline; version-gated IDs).
- [ ] README: state the PC-only/SFSE position and the death-boundary philosophy explicitly.

## Milestone 1 — Trust Core ✅ (implemented 2026-07-01, runtime verification pending)

The features that make OSF Defeat *more* trustworthy than Acheron. All native, no RE blockers.

1. ~~**Ownership lease + watchdog.**~~ Done: `ClaimDefeat(actor, owner, maxHoldSeconds)` /
   `ReleaseClaim` / `GetClaimOwner` verbs; re-claim by the same owner extends, a competing owner is
   refused; `holdRecovery` auto-claims under the consequence name (`maxHoldSeconds` JSON field);
   lapsed deadline → force-rescue + WARN naming the defaulter. Player held entries are always
   watchdog-armed (`watchdogSeconds`, default 600, 0 disables); NPC held entries only when leased
   (a held captive is a legitimate long-term state). Lease persists in co-save (record v3).
   Quest-stop detection is deferred — TESQuest state reads are RE-blocked; the deadline covers it.
2. ~~**Emergency escape.**~~ Done: `ForceRecoverAll()` verb, console-callable via
   `cgf "OSFDefeat.ForceRecoverAll"`. Overrides any lease. (Held-key binding deferred — needs an
   input hook; the console path covers the unstick case.)
3. ~~**Decision transparency ("explain mode").**~~ Done: `explainDecisions` validation flag adds
   NPC passthrough reasons and full selection detail (per-outcome filter reason: disabled /
   off-domain / cooldown / once-per-location / aggressor-or-victim conditions; then the
   priority/weight pool and the roll). Filter reasons for outcomes log at INFO always.
4. ~~**Safe uninstall / recover-all.**~~ Done: `ForceRecoverAll` also releases pacified actors and
   strips framework-created captive factions; the co-save empties on the next save.
5. ~~**Legible death boundary.**~~ Done: every deliberate player passthrough logs at INFO with its
   reason (API-disabled, config-disabled, in-scene, excluded location, already dead), debounced.

## Milestone 2 — Schema v2: Arbitration + Domains ✅ (implemented 2026-07-01)

Cheap now, painful to retrofit once content packs exist in the wild.

- ~~Per-consequence `cooldownSeconds`, `oncePerLocation`, and `enabled`.~~ Done. Cooldown and
  once-per-location state is session-scoped by design (steady clock + fired-cell set).
  `oncePerLocation` keys on the **parent cell** (GetCurrentLocation is RE-blocked); revisit when
  the location seam is proven.
- ~~Central user overrides file.~~ Done: `SFSE/OSFDefeat/Overrides.json` overrides
  `enabled`/`weight`/`priority` per consequence name; unknown names log a hint; applied on load
  and `ReloadJSON`.
- ~~`domain` field.~~ Done: `"onfoot"` default, `"ship"` parses but never competes (parked with a
  log line); *unknown* domains also park rather than misfire on foot. v1 files stay valid.
- ~~Schema `version: 2` with v1 accepted.~~ Done for consequence + validation JSON; unknown
  top-level consequence fields warn with file + field (`//`-prefixed keys stay silent for
  comment conventions).

## Milestone 3 — Default Consequence Pack (in the box) — v0.1 shipped

Don't repeat Acheron's empty-by-default mistake; make the framework worth installing alone, and
prove the M1 lease/handoff API as its own first consumer.

**Decision made: companion ESM** (`pack/` → `OSFDefeatBasic.esm`, built from Spriggit YAML by
`pack/Build.ps1`, deployed as its own MO2 mod `OSF Defeat - Basic Consequences`). The framework
DLL stays pure; the pack demonstrates the real content-mod workflow.

- ~~**"Robbed and Left"** (v0.1, 2026-07-01)~~: human aggressors take a credits cut (default 10%,
  min 25 / cap 2500) **into their own inventory** — hunt them down to recover it. `holdRecovery`
  + auto-claim + `maxHoldSeconds: 120` watchdog, `cooldownSeconds: 300` arbitration, explicit
  rescue handshake. Optional `HurtAffliction` spell property (ships unfilled).
- Remaining for M3:
  - Fill the affliction on the shipped record (needs verified vanilla affliction spell FormIDs).
  - **Arrest path** for security/guard-faction aggressors via `Faction.SendPlayerToJail`
    (SAF-proven; needs the faction forms + a bounty-semantics test).
  - Relocation ("wake at the nearest civilized point") once placement validation is designed,
    plus persistence promotion for transient aggressors (the Shadow of Skyrim lessons).
  - Condition grammar: gate outcomes on `defeatCause` (EM-downed → different default beat).

## Milestone 4 — NPC Defeat Identity: EM + Capture

Give NPC defeat a vanilla-plausible fantasy: stun-and-capture bounty hunting.

- ~~`defeatCause` in the handoff context.~~ Done (2026-07-01): the clamp records Health vs EM per
  gauge, the `DefeatActor` verb records scripted; persisted (co-save v4); exposed as
  `GetDefeatCause(actor)` and `GetLastPlayerDefeatCause()` (0 none / 1 health / 2 EM / 3 scripted).
- **NPC consequence policy**: what happens to a defeated NPC beyond down/recover — default policies
  (recover when combat ends; plunderable while down; executable by aggressor) with content override.
- Capture/ransom **hooks** for content (claim-a-captive verb, transfer-to-marker helper) — the
  framework provides verbs, content provides the bounty board.
- Follower/companion policy: companions get follower-specific down behavior, never auto-executed,
  excluded from `npcAutoDefeat` by default.
- Harden `npcAutoDefeat` for default-on candidacy.

## Milestone 5 — Ship Domain

The most Starfield-native leapfrog: the player's disabled ship gets boarded instead of exploding.

- RE the ship-death/disabled seam (hull-zero and engine-disabled paths) in the OSF RE workspace.
- Ship defeat state machine (disabled → boarded → consequence) reusing the existing selection +
  handoff machinery under `"domain": "ship"`.
- Default ship consequence in the M3 pack: boarded + robbed (cargo/credits) + ship left adrift or
  towed, crew/companions handled by policy.

## Second Tier / Platform Plays

- ~~**Struggle primitives**~~ Done (2026-07-01): `OnPlayerDefeatImminent` callback fires below
  `imminentHealthFraction` × max health (default 0.25, hysteresis re-arm); `ResistDefeat(actor,
  seconds)` opens a per-actor no-defeat grace window (vanilla lethality while active — a
  successful struggle means you fight on and can genuinely die), player and NPC both.
- **Interaction registry on defeated NPCs** (Hunter Pride analog): content mods register options
  with conditions; framework renders via dialogue/scene (no SkyUI equivalent). This is the
  "defeated NPCs as a platform" play.
- **Condition grammar completion**: `locations` / `questsRunning` / `questsCompleted` — blocked on
  RE (GetCurrentLocation ID unmapped; TESQuest state natives unbound).
- Alias/reference **fill** on consequence quest start (carried over; open).

## Alpha Test Plan

- Lethal player hit causes defeat instead of death.
- Player recovers after configured `recoverySeconds`.
- Combat resumes cleanly after recovery (no pacify, no freeze, no `tcai` needed).
- Save/load while defeated preserves and resumes defeat state.
- Invalid JSON logs an error and falls back safely.
- `DisableProcessing(true)` lets vanilla death/behavior proceed without OSF handling.
- A consequence that never calls back gets force-rescued by the watchdog (M1).
- `xmake build` passes and deploys DLL/PDB, Papyrus files, and JSON defaults.

## JSON Schemas

Validation v2 (current; v1 files stay valid):

```json
{
  "version": 2,
  "processingEnabled": true,
  "disableInScenes": false,
  "defaultRecoverySeconds": 8.0,
  "npcAutoDefeat": false,
  "npcAutoDefeatIncludeRobots": false,
  "factionPacify": false,
  "captiveFaction": "0x0003E0C8",
  "participantScanRadius": 4096.0,
  "explainDecisions": false,
  "watchdogSeconds": 600.0,
  "excludedLocations": [],
  "excludedQuests": []
}
```

Player consequence v2 (current; v1 files stay valid):

```json
{
  "version": 2,
  "name": "Alpha Down Then Recover",
  "priority": 0,
  "weight": 1,
  "recoverySeconds": 8.0,
  "quest": "OptionalQuestEditorID",
  "startQuest": true,
  "holdRecovery": true,
  "maxHoldSeconds": 0.0,
  "enabled": true,
  "cooldownSeconds": 0.0,
  "oncePerLocation": false,
  "domain": "onfoot",
  "conditions": {
    "aggressor": { "keywords": ["ActorTypeHuman"], "factions": [], "match": "any" },
    "victim":    { "keywords": [], "factions": [], "match": "all" }
  }
}
```

A `quest` token that resolves (editorID first, then hex FormID) gets `Quest.Start` dispatched
before the consequence event fires; `startQuest: false` opts out. `holdRecovery: true` skips the
auto-recovery timer and auto-claims the down-state under the consequence's name (`maxHoldSeconds`
sets the watchdog deadline; 0 = config `watchdogSeconds`) — the consequence owns the down-state
and ends it via `RescueActor`, `ReleaseClaim`, or `SetRecoveryTimer`.

User overrides (`SFSE/OSFDefeat/Overrides.json`):

```json
{
  "version": 1,
  "consequences": {
    "Some Pack's Outcome": { "enabled": false },
    "Another Outcome":     { "weight": 5, "priority": 10 }
  }
}
```

`conditions` gates whether an outcome competes at all: outcomes whose conditions fail for the
current victim/aggressor pair are filtered before the priority/weight roll (no eligible outcome →
built-in fallback). Keyword/faction entries are form tokens (editorID or hex FormID); `aggressor`
defaults to `"match": "any"`, `victim` to `"all"` — Acheron's semantics. A non-empty aggressor
block fails when there is no aggressor. `locations` / `questsRunning` / `questsCompleted` are
schema-reserved but ignored until their engine seams are RE-proven.

## Non-Goals (content-mod territory)

Scenes and animation, stripping/robbery item policy, ransom/bad-end/device systems, capture
dialogue and captivity gameplay, per-content configuration UIs. The framework provides the
down-state, the handoff, the pacify verbs, and the context — content provides the meaning.
Reference consumers: the M3 default pack (SFW) and OSF Surrender (adult) prove the same API.
