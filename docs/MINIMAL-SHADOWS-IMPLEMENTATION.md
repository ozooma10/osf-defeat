# Minimal Shadows implementation

Slice 1 of [CORE-EXPERIENCE.md](CORE-EXPERIENCE.md), implemented as a separate
[staged pack](../shadows/README.md). Basic Consequences remains the shipped pack.
The disposable-cell acceptance suite passes; existing-enemy world acceptance
remains open below.

## Implemented loop

- One persistent actual-aggressor alias, titled **The Unbroken**, with a hunt objective.
  Another defeat cannot replace an occupied slot, steal again or remove its debuff.
- **Unbroken** gives the nemesis +40 physical resistance; **Shaken** gives the player
  -20. A player kill earns **Hardened Resolve** (+10) only when the target is at least
  the player's level at death. Another killer clears the debuff without that reward.
  These are pack-owned abilities, not curable vanilla afflictions.
- Conservative admission excludes essential/protected/unique actors, quest aliases,
  protection overrides, teammates, commanded actors, children, ghosts, scenes and
  targets more than 15 levels above the player. Retained alias metadata and even a
  cleared reference protection override are excluded.
- Promotion takes 10% of credits, clamped to 25–2,500 and available funds. Weapon
  theft uses the pre-down equipped-base snapshot, requires exactly one owned copy,
  skips quest items, compares base credit value, and moves the inventory item to the
  nemesis. This quality proxy does not evaluate damage or modifications. Suit,
  helmet and boost pack are never taken.
- The aftermath fades out, advances at least two and less than three game hours,
  then recovers in place with a 30-second truce. PassTime rounds to an hour boundary,
  so the script corrects a short advance with one additional hour. A 12,000-second
  game-clock lease accommodates the skip; a separate 30-second Papyrus timer covers
  stalled work. Generation checks invalidate old latent stacks after cancellation.
  External shutdown or reload during blackout requests recovery and restores the
  fade, preserving an already-created hunt instead of starting the consequence again.
- Ship interiors and space refuse this loop, restore framework state, then request
  normal death. This is a staged content-level refusal, not a native domain gate.

`tools/build-shadows.ps1` compiles the scripts, builds and round-trips the nine-record
ESM with pinned Spriggit, then independently checks record links, aliases, objectives,
spell magnitudes and VMAD bindings. It does not deploy. `-WorldProbe` adds only a test
quest and identity Actor Value; the default payload excludes both diagnostics.

## Recovery truce foundation

`RecoverActorWithTruce(actor, seconds)` atomically recovers a defeated actor while
retaining captive-faction pacification. A finite positive duration up to 3,600 seconds
becomes a saved game-day deadline. It does not grant damage immunity. Query
`IsRecoveryTruceActive` as well as `IsDefeated` to confirm the queued request completed.
Missing pacification leaves recovery held for the existing watchdog.

Outgoing health damage queues truce cancellation after the original damage write,
with the save-epoch guard. Shadows also watches player fire and graph callbacks for
misses. Actual frag release emits `WeaponFire`; guessed `ThrowGrenade` callbacks did
not fire. `HitFrame` covers melee/unarmed misses. Scripted attack producers that
bypass these events must call `EndRecoveryTruce` explicitly. New defeat and
`ForceRecoverAll` end the truce. Pre-existing faction memberships are preserved;
unresolved actors retain cleanup tracking. `TRC0` v1 saves the deadline and ownership;
`CPT0` retains the exact faction ID. See the [Papyrus API](papyrus-api.md).

Loading a saved blackout exposed an OSF Animation input-layer leak: defeat recovery
completed but movement/fighting stayed disabled. Animation commit `fcf9714` reconciles
only its owned layer after load, retaining any new-world lock. World profile preparation
copies the built Animation DLL privately alongside Defeat so it includes this fix.

## Validation — 2026-10-17

- Focused live suite: **28/28 assertions**, fresh owned local MO2/SFSE session.
  Defeat revision `624ec08`, Animation `fcf9714`; receipt
  `build/shadows-acceptance-11.json`; artifacts
  `../OSF Test Harness/artifacts/20261017-052332-928-DefeatSmoke`.
- Full regression: **66/66 live assertions**, **83 native cases / 1,014 checks**,
  **97 runner checks**, payload validation, production build and source-integrity
  checks pass on Defeat `c16498a` with Animation `fcf9714`. Receipt:
  `build/smoke/20261017-053511-42d736e793ad49edb4bbb3b4ba63dd98/summary.json`;
  artifacts: `../OSF Test Harness/artifacts/20261017-053519-156-DefeatSmoke`.
- Animation's **12 engine-free suites** pass (`build/shadows-animation-tests-01.log`).
- World probe compiles and its 11-record ESM round-trips and verifies. Receipt:
  `build/shadows-world-02/build.json`. **21 isolated-profile preparation checks** pass;
  `build/shadows-world-preparation-04.log`. Preparation is not runtime acceptance.

The live fixtures cover the ordinary one-slot loop and resistance magnitudes; a
second lethal defeat while occupied; player reward, other-killer cleanup and a
lower-level enemy refusing a reward; reference protection overrides, teammate,
ghost and unrelated-alias rejection; truce save/load/expiry; missed knife, unarmed,
Eon and frag attacks; a cooperating scripted attack; external quest interruption;
and an early-blackout save/reload with control, credits, time and empty-hunt checks.

The weapon fixture attaches a suppressor and Instigating, Rapid and Explosive
legendary slots to one Eon. A read-only, version-gated native probe compares every
OMOD row, including attachment metadata and duplicates, at baseline, theft,
real reload, corpse retrieval and explicit player drop/pickup. All match. That last
step certifies the forced world-pickup path. The focused run compared 24 rows and
the full run compared 19; each preserved its own exact baseline. A prior full run
found the weapon absent from corpse inventory and a nearby Eon with a different
reference from the stolen inventory alias. The revised test now requires exact
modification data before accepting a natural drop. The passing rerun took the
corpse path, so the natural-drop branch remains unverified.
Native-layout and graph-event evidence lives in OSF RE's `gameplay.actor_equipment`,
`engine.animation_graphs` and `ui.input_enable_layer` modules. Failed iteration
receipts are retained rather than overwritten.

## Remaining acceptance before replacing Basic

- Existing world aggressor at an authored combat encounter and a generated enemy
  structure: normal full-body appearance, title/journal/abilities, normal planet
  departure and return, reload/cold restart, long absence and actual reset behavior.
  The old seeds prove inert surrogates at a city and a natural feature only.
- Naturally dropped NPC weapon retrieval, other weapon/throwable families and
  non-cooperating third-party attack producers.
- Ship/space refusal fixtures and blackout interruption after theft/time advancement.
  The passing blackout reload is within one process and before promotion.

The [world probe and setup guide](../tests/world-shadows/README.md) are ready. It
stamps an existing enemy once, relies on the production hunt's alias, observes
unload/reattach and live cells, and records actual elapsed UT time and any alias
reset events. It neither suspends AI nor adds another subject alias. A long wait
alone does not certify a reset. Two new disposable combat saves are still required.

The wake-up table remains the next slice after these world gates. The five-slot
library, dropped pack, companion rescue, settings and wider traits follow later.
Before disabling this test pack, call `CancelHunt`, then `ForceRecoverAll`, verify
cleanup and save. Stolen loot remains where it was transferred.
