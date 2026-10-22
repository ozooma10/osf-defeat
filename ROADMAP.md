# OSF Defeat Roadmap

Direction updated 2026-10-21. This roadmap defines the first framework release and
its separate content tracks. Implemented features and historical test passes are
evidence toward release; they do not mean the release gates below are complete.

## Purpose

**Give Starfield content mods a dependable way to turn defeat into different kinds
of gameplay without each author rebuilding death prevention, recovery, and compatibility.**

OSF Defeat is the framework. Shadows is an optional content pack built on its public
API, with its own gameplay decisions, validation, and release schedule. The framework
is judged by whether authors can build different experiences on it and users can
trust those experiences to finish cleanly.

The first release is PC/SFSE-only and covers on-foot actor defeat. It does not promise
support across untested game versions or every death mechanism. Publish the exact
Starfield, SFSE, Address Library, and OSF Animation requirements with the release.

## Product boundaries

| Product | Responsibility | Release relationship |
| --- | --- | --- |
| **OSF Defeat** | Defeat/recovery state, context, player consequence selection, handoff, persistence, and cleanup of framework-owned changes. | Independently installable; includes a plain down/recover fallback. Requires its documented runtime dependencies, including OSF Animation. |
| **Basic Consequences** | A small optional starter pack and readable integration examples: robbery, injuries, custody, and recovery. | Separate ESM/payload, offered alongside the framework; replaceable without requiring Shadows. |
| **Shadows** | Personal nemeses, theft, blackout/wake-up, hunt objectives, and any pack-specific traits or rewards. | Separate optional ESM/payload depending on OSF Defeat; its content milestones do not gate the framework release. |

The framework owns the technical transition. Content owns its meaning: what is taken,
where someone wakes, whether a wound can be treated, how a hunt ends, and what a player
earns. Framework emergency recovery cannot undo arbitrary changes made by a content
quest; each pack must provide its own interruption and cleanup behavior.

A persistent story can continue after recovery without retaining a defeat lease.
Shadows' hunt belongs to Shadows for that entire lifetime.

## Goals

1. **Reliable defeat and recovery.** Restore framework-owned state after normal recovery,
   interrupted content, and save/load. Preserve unrelated state and report unresolved cleanup.
2. **Useful, honest context.** Expose the defeated actor, attributed aggressor, defeat cause,
   and recent hostile attackers. Document missing information and attribution fallbacks;
   do not invent a confirmed killing blow, location, or damage type.
3. **Predictable cooperation.** Authors can register content, determine eligibility, acquire
   a recovery hold, and finish it. Users can control which installed consequences compete.
4. **Different gameplay on the same foundation.** Rescue, robbery, custody, captivity,
   nemesis hunts, and NPC interactions can make different policy choices.
5. **An approachable authoring path.** Public scripts, JSON examples, lifecycle guidance,
   and actionable diagnostics make a first integration possible without editing the DLL.

New core capabilities need a concrete consumer and a reusable responsibility. A
feature being useful to Shadows does not by itself make its gameplay policy a core rule.

## First framework release: in scope

| Area | Release scope |
| --- | --- |
| **Player defeat** | Intercept supported lethal Health damage; expose scripted defeat and processing/resistance controls; enter one coherent down-state and recover with working movement, camera, and combat input. |
| **NPC primitives** | Public defeat/recover, actor events, AI suspension/restoration, captive-faction and capture/release/query verbs for validated humanoid contexts. These let content manage NPCs without imposing a default capture story. |
| **Recovery and ownership** | Automatic recovery, held recovery, exclusive lease acquisition/renewal/release, default-enabled watchdog, and console emergency recovery. Existing post-recovery truce remains an explicit API with documented attack-integration limits. |
| **Player consequence selection** | Stable IDs, enabled/priority/weight, actor keyword/faction and cause conditions, cooldowns, once-per-cell behavior, user overrides, JSON reload, and safe fallback when nothing is eligible. |
| **Handoff and context** | Optional quest start, player and actor callbacks, existing context getters, and conservative actor-reservation queries. Authors confirm resulting state before dependent actions; native mutation return values indicate request acceptance. |
| **Persistence and cleanup** | Save/load and 3D reattachment of framework state; no work from a previous save mutating a new one; ownership-aware cleanup, with unresolved references retained and reported for retry. |
| **Author and user support** | Versioned JSON/Papyrus contract, existing compatibility aliases, clear logs, exact dependency/support matrix, a minimal example, and recovery/disable instructions that distinguish core cleanup from pack cleanup. |

### Boundaries within that scope

- Consequence JSON arbitration is **player-only** in the first release. NPC consumers
  use actor events and verbs; a general NPC consequence selector is later work.
- `npcAutoDefeat` stays **off by default and experimental**. Existing opt-in behavior
  remains available with its tested limits; default-on balance, companion policy,
  and nonhuman down/capture presentation are not first-release promises.
- Existing capture transfer and other advanced verbs remain available, but support
  claims must name the tested contexts. Cross-cell/ship captive transport needs its
  own acceptance before being advertised as supported gameplay.
- Defeat causes are **Health or scripted**. EM/stun-driven defeat is not implemented.
- Ship piloting, space combat, hostile boarding, and universal scripted-death interception
  are outside the supported first-release scope. A reserved `domain: "ship"` JSON field
  is not a working ship-defeat gate or ship-defeat implementation.
- Cooldowns and `oncePerLocation` tracking are currently session-scoped;
  `oncePerLocation` means parent cell. Preserve and explain that behavior for this release.
- Location and quest conditions/exclusions are not supported authoring capabilities.
  Invalid or unsupported consequence conditions must not broaden a consequence's
  eligibility. Global exclusion settings must not be presented as working protection;
  the release guide must state their actual behavior and supported processing controls.
- The watchdog is enabled by default. Disabling it removes timeout protection;
  unleased NPC recovery holds are intentionally not timed out. A lease coordinates
  cooperative consumers, while emergency recovery can override it.
- Retain the existing imminent-defeat/resistance primitives without adding a built-in
  surrender prompt or struggle minigame. Resistance permits vanilla lethal damage.

## Implementation baseline

The foundation already exists. First-release work is primarily closing evidence and
integration gaps, fixing failures, and documenting the contract rather than adding systems.

| Area | Current evidence / limitation |
| --- | --- |
| Native framework, schema v3, aliases, selection, holds, capture, events, persistence | Implemented; native and runtime coverage is indexed in [tests/README.md](tests/README.md). Offline tests do not validate game addresses or real engine behavior. |
| Basic Consequences | Six scenarios exist in a separate pack. [TESTPLAN.md](TESTPLAN.md) records controlled live combat and recovery evidence; full custody accounting and broader natural-encounter coverage remain distinct gaps. |
| Shared additions exercised by Shadows | Truce, actor-reservation checks, interruption and save/load behavior have focused evidence. [API documentation](docs/papyrus-api.md) describes limitations, including custom attack producers. |
| Minimal Shadows | One-slot hunt, theft, blackout, traits, recovery truce, and cleanup are staged and have controlled-fixture evidence. [Implementation record](docs/MINIMAL-SHADOWS-IMPLEMENTATION.md) separates that evidence from open world acceptance. |

Use retained receipts for the exact candidate being released. Earlier passes do not
certify later source changes, and a long wait alone does not prove a cell reset.

## First framework release gates

These are release sign-off items, not a statement that each behavior is unimplemented.
Use the existing native, payload, runtime, and combat checks; add coverage only where
the supported contract lacks meaningful evidence.

### F1 — Dependable lifecycle

- [ ] Build and package a candidate with exact, obtainable runtime dependencies and
  verified hook/version checks. Unsupported versions must fail safely.
- [ ] Verify lethal and scripted player defeat, repeated defeat, automatic and held
  recovery, and movement/look/aim/fire afterward in supported on-foot combat contexts.
- [ ] Verify normal death when processing is disabled or resistance permits it, and
  document the tested death boundary without claiming universal interception.
- [ ] Verify supported humanoid NPC defeat/recovery and capture/release paths, including
  restoration of prior AI/faction state and cleanup of the capture relationship.
- [ ] Verify competing lease refusal, extension/release, a consumer that never finishes,
  failed or missing quest handoff, and emergency recovery. Recovery must remain possible
  even when content fails; unresolved cleanup must be visible.

### F2 — Persistence and interruption

- [ ] Verify save/load and cold restart during an automatic defeat and a leased hold,
  including deadlines and working controls after recovery.
- [ ] Verify loading another/older healthy save, interrupted or failed load handling,
  and 3D unload/reattach without stale mutations, duplicate consequences, or leaked state.
- [ ] Verify the supported missing/unresolved-actor cleanup path. Publish the recovery
  procedure; do not describe `ForceRecoverAll` as unconditional removal of all pack data.
- [ ] Verify existing truce expiry, attack cancellation, and save/load for the supported
  attack paths; document producers that must call `EndRecoveryTruce` explicitly.

### F3 — Multiple consumers and authoring

- [ ] Exercise two materially different consumers against the same candidate: Basic's
  short aftermath and staged Minimal Shadows' longer-lived hunt are suitable fixtures.
  Check each alone, both registered, neither installed, and one disabled by override.
- [ ] Verify filtering, priority/weight selection, cooldowns, stable IDs, overrides,
  JSON reload, and fallback. Malformed or unsupported authored conditions must not run
  as an unrestricted consequence.
- [ ] Confirm that only the selected player consequence begins its aftermath, that
  callbacks do not imply ownership, and that releasing a defeat lets a pack's independent
  quest continue without retaining control of the actor's down-state.
- [ ] Publish a minimal JSON + Papyrus example and lifecycle checklist covering request
  acceptance versus completion, lease renewal, recovery, interruption, and pack cleanup.
- [ ] Reconcile README, API documentation, and release notes with the supported matrix,
  default settings, current dependencies, known gaps, and evidence for this candidate.

The second consumer is an **integration fixture** for the framework release. Shadows'
full gameplay release, title library, wake-up system, and world-persistence certification
are not required to sign off that limited integration check. Shared API failures found
by a pack still block the affected framework contract until fixed or explicitly excluded.

## Basic Consequences: optional starter pack

Keep the existing small pack as the default content option and reference consumer.
Its separate installation remains replaceable; the core must function without it.

For its accompanying release:

- Validate the advertised short aftermath/recovery behavior, including tagged-marker
  relocation and the no-marker fallback. The current fallback recovers in place;
  it does not promise a safe destination or a persistent revenge objective.
- Verify inventory deltas and any custody behavior being advertised. Narrow or disable
  an unverified scenario rather than treating broad custody gameplay as a core requirement.
- Explain that loot transferred to a transient aggressor is not a guarantee that the
  actor and loot remain retrievable after leaving the area.
- Keep new stripping libraries, repeated-defeat escalation, and wider destination
  coverage as later pack improvements.

## Shadows: independent content track

Shadows explores the player-facing loop: **defeat → aftermath → recovery → hunt**.
It uses the same public API as another author's content and does not replace Basic as
a framework requirement. [CORE-EXPERIENCE.md](docs/CORE-EXPERIENCE.md) is its broader
content proposal; this roadmap supersedes that proposal's earlier packaging decisions.

### S1 — Prove the existing minimal hunt

- Keep the first candidate to one active rival, a limited theft policy, a readable
  objective, and complete success/interruption/abandonment cleanup.
- Validate promotion of an actual enemy in an authored combat site and a generated
  enemy structure: identity, presentation, inventory, planet departure/return,
  save/load/cold restart, long absence, and actual reset behavior. Controlled surrogate
  fixtures do not certify those paths. See the [world probe](tests/world-shadows/README.md).
- Define and verify what happens when a rival or site becomes unavailable, and make
  limitations on recovering stolen possessions explicit.
- Settle pack policy for injuries, treatment, rewards, and abandoning a hunt. The current
  revenge-only wound and stat bonuses are prototype choices, not framework requirements
  or newly approved changes to the implementation.

### S2 — Make aftermath worth playing

- Add and validate a small set of plausible wake-up destinations and a usable fallback.
  Prioritize a safe opportunity to continue over broad destination coverage.
- Validate the complete player experience, including repeated defeat and choosing to
  leave a hunt unresolved. A fixed truce timer alone is not proof of enjoyable recovery.
- Publish Shadows' own support limits, dependencies, cleanup procedure, and release evidence.

### S3 — Expand after the loop works

Additional rival slots, archetype titles/traits, dropped packs, companion rescue,
authored captivity, presentation, and pack-specific settings are later content milestones.
They do not extend the first framework release checklist.

## Deferred framework work and non-goals

| Later work | Why it stays outside the first release |
| --- | --- |
| Ship defeat and boarding | A new engine domain and lifecycle with its own research and acceptance requirements. |
| General NPC consequence arbitration and interaction menu | New routing/UI contracts; existing actor verbs and callbacks are the initial authoring surface. |
| Default-on NPC defeat and companion policies | Requires broader presentation, compatibility, and gameplay decisions. |
| EM/stun-triggered defeat | Requires a proven engine interception path. |
| Location/quest condition completion, richer defeat-site and killing-blow context, automatic quest-alias filling | Add when a concrete consumer requires the capability and its engine behavior can be validated. |
| Built-in surrender UI, struggle minigame, or settings UI expansion | Existing primitives and JSON configuration cover the first release contract. |

Faction simulation, NPC biographies, nemesis hierarchies, ransom/captivity stories,
item-taking rules, injury/reward balance, and authored scenes belong to content projects.
enRage and Shadow of Skyrim are gameplay references, not framework feature checklists.

## Working references

- [Public API and lifecycle semantics](docs/papyrus-api.md)
- [Papyrus declarations](dist/Scripts/Source/OSFDefeat.psc)
- [Canonical vocabulary and compatibility aliases](docs/VOCABULARY.md)
- [Settings template](dist/SFSE/OSFDefeat/Settings/default.json) and [JSON examples](README.md#json)
- [Basic Consequences](pack/README.md) and [staged Shadows](shadows/README.md)
- [Framework test inventory](tests/README.md), [combat evidence](TESTPLAN.md), and
  [Shadows acceptance record](docs/MINIMAL-SHADOWS-IMPLEMENTATION.md)
