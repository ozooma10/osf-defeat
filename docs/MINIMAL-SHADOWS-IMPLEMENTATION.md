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
  nemesis. If it has no compatible base ammunition, promotion supplies one round
  so its AI keeps the stolen gun equipped. This quality proxy does not evaluate
  damage or modifications. Suit,
  helmet and boost pack are never taken.
- The aftermath fades out, advances at least two and less than three game hours,
  then recovers in place with a 30-second truce. PassTime rounds to an hour boundary,
  so the script corrects a short advance with one additional hour. A 12,000-second
  game-clock lease accommodates the skip; a separate 30-second Papyrus timer covers
  stalled work. Generation checks invalidate old latent stacks after cancellation.
  External shutdown or reload during blackout requests recovery and restores the
  fade, preserving an already-created hunt instead of starting the consequence again.
  Reload aborts invalidate the saved stack before bounded polling for native state
  restoration and recovery. A load-time empty registry or refused task submission
  no longer counts as recovery; generation checks also guard the final quest stop.
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

- Full regression: **78/78 live assertions**, four owned local MO2/SFSE
  processes, **83 native cases / 1,014 checks**, **103 runner checks**, payload
  validation, production build and source-integrity checks pass. Defeat `5dc8955`,
  Animation `fcf9714`; receipt `build/shadows-final-regression-08/summary.json`;
  artifacts: `../OSF Test Harness/artifacts/20261017-133752-565-DefeatSmoke`.
- Focused post-theft/post-time suite: **13/13 assertions**, three fresh processes,
  Defeat `aa1c7c9`. Receipt `build/shadows-checkpoints-05.json`; artifacts
  `../OSF Test Harness/artifacts/20261017-133250-510-DefeatSmoke`.
- Uninstrumented ship/space refusal: **8/8 assertions** in the retained Frontier
  fixture, Defeat `5dd4eff`. Receipt `build/shadows-refusal-02.json`; artifacts
  `../OSF Test Harness/artifacts/20261017-131715-750-DefeatSmoke`.
- Animation's **12 engine-free suites** pass (`build/shadows-animation-tests-01.log`).
- World probe compiles and its 11-record ESM round-trips and verifies. Receipt:
  `build/shadows-world-04/build.json`. **21 isolated-profile preparation checks** pass;
  `build/shadows-world-preparation-06.log`. Preparation is not runtime acceptance.
- The test snapshot publisher survives a **1.5-second replacement-denying file lock**
  spanning its 500 ms update cadence; it retries without blocking Main and retains
  a five-second failure deadline (`build/shadows-snapshot-lock-02.json`).

The live fixtures cover the ordinary one-slot loop and resistance magnitudes; a
second lethal defeat while occupied; player reward, other-killer cleanup and a
lower-level enemy refusing a reward; reference protection overrides, teammate,
ghost and unrelated-alias rejection; truce save/load/expiry; missed knife, unarmed,
Eon and frag attacks; a cooperating scripted attack; external quest interruption
before and after promotion; and an early-blackout save/reload with control, credits,
time and empty-hunt checks in both the same process and a cold restart. The later
interruption was observed in phase 2, with one credit deduction, retained hunt/traits,
working controls and about 2.03 game hours elapsed. Cold restart uses the exact
hashed save pair in a new private save directory and verifies distinct process
creation times and launch tokens (`runtime-sessions.json`). It requires the saved
Papyrus consumer and its saved baselines rather than creating a replacement.

The [expanded acceptance runner](../tests/runtime/README.md) also saves after theft
and after time advancement, loads each in the same process, then boots each exact
pair in a fresh process. Test-only gates pause those brief phases at the production
generation guards; the ordinary builder and ship-refusal suite have no gates.
Removing the two test loops must reproduce the production source byte-for-byte
after newline normalization. This proves save/load behavior at the boundaries,
not that manual console saves can reliably hit them at normal timing.

Both later checkpoints retain the hunt/objective/traits, exactly one credit
deduction, and the stolen weapon's complete OMOD rows. Two inspections three
seconds apart reject a delayed replay of theft/time. The focused run preserved
24 rows after theft and 22 after the approximately 2.01-hour skip, including cold
restart. Aliased inventory proxies can report no container: ownership comes from
the actual inventories and exact metadata, while the alias/base is checked
separately. Sidecar inspection requires valid CRCs and exactly one defeat plus
one AI suspension. Earlier disposable subjects are resumed and removed before
the next checkpoint. Phase writes retain generation guards before and after them.

The Frontier suite verifies a landed interior, then the actual home ship's
`TakeOff()` transition to an orbital interior (`IsLanded=False`, both space
predicates true). Each lethal hit selects the production refusal, preserves
credits/weapon metadata/time, creates no hunt/traits/truce, clears the held state,
and ends in vanilla death followed by an automatic healthy reload. Animation's
retained log confirms `osfdefeathealthy.osf` loaded twice. Controls and empty
Defeat registries pass after each load. The CommonLib dead-bit predicate stayed
false while Papyrus reported death, so the oracle retains Papyrus dying/death
events and `IsDead`, plus the real load event and epoch. The narrow observation is
recorded in OSF RE's `gameplay.defeat_damage_hook`; no new native offset is claimed.
This covers on-foot refusal in that ship, not piloting or hostile boarding layouts.

Cold-restart preparation closes the idle test MO2 launcher before changing its
private save path, writes a sibling INI, and atomically replaces the old file with
a bounded retry. Deliberate file-lock checks prove failed replacement preserves
the original and a released lock permits replacement. Earlier failed/blocked run
receipts remain retained, including the corrected inventory-proxy assertion and
the MO2 INI lock; they are not counted as passes.

Two acceptance failures exposed production issues. A stolen Eon with no compatible
NPC ammunition was replaced by fists after AI resumed; the new fixture starts with
zero rounds and verifies exactly one after promotion. A cold blackout reload stopped
the quest while native recovery was not ready, leaving the player down and locked;
bounded restoration/recovery polling now passes without replaying theft or time.

The weapon fixture attaches a suppressor and Instigating, Rapid and Explosive
legendary slots to one Eon. A read-only, version-gated native probe compares every
OMOD row, including attachment metadata and duplicates, at baseline, theft,
real reload, natural death drop, retrieval and explicit player drop/pickup. All match:
30 rows in the latest full run, each compared with its exact pre-theft baseline. The NPC's AI is resumed and its equipped weapon drawn before death; the
test does not force an NPC DropObject. The natural drop has a different reference
from the inventory alias, so the probe requires exact modification data before
accepting it. The earlier full run `20261017-053519-156-DefeatSmoke` covered corpse
retrieval. Explicit player DropObject/pickup remains a separate tested path.
Native-layout and graph-event evidence lives in OSF RE's `gameplay.actor_equipment`,
`engine.animation_graphs` and `ui.input_enable_layer` modules. Failed iteration
receipts are retained rather than overwritten.

## Broader weapon acceptance (2026-10-17)

The separate `-WeaponsOnly` matrix passes **31/31 assertions** on Defeat `1d66618`
with uninstrumented production Shadows scripts and Animation `fcf9714`. It covers
26 attacks: Eon, Grendel, Coachman, Breach, Equinox, Va'ruun Inflictor, Magpulse,
Magstorm, Microgun, Bridger, Novablast, Cutter, combat knife, rescue axe, Wakizashi,
unarmed, Eon bash, frag/impact/shrapnel/particle/incendiary grenades, and
frag/inferno/cryo/Tesla mines. Equipment setup and reload/aim input preserve the
truce; each attack ends it before its deadline and clears captive-faction ownership
and membership. Guns consume ammo, except the energy-recharging Cutter; each
throwable consumes exactly one item. The bounded graph observer records WeaponFire
for all ranged/throwable cases and HitFrame for melee/unarmed/bash, alongside the
production quest's fire/animation callbacks.

Receipt: `build/weapons-03.json`; artifacts:
`OSF Test Harness/artifacts/20261017-142007-594-DefeatSmoke`. The owned process was
PID 44652, creation FILETIME `134367204364080996`. Ten isolation loads and final
cleanup produce eleven observed load events and epoch 13. The final healthy load
restores controls, empty framework state, original gear/health, and the original
camera view; the game closes normally. Offline validation on the same revision
passes 83 native cases / 1,014 assertions, 132 runner checks, payload verification,
and the production build (`build/weapons-offline-01/summary.json`).

No production change was necessary. Retained iteration 01 rejected an input hold
above the helper's one-second limit; the corrected action durations now have a
runner contract. Iteration 02 passed shrapnel release, then lost its next truce
before input, consistent with lingering explosive damage. Each throwable now
starts from a verified healthy load, as does the case after the Bridger, preventing
effects from contaminating subsequent cases. Mid-session failures also retain a
capture even when the session has not returned successfully.

This is first-person keyboard/mouse coverage of the listed forms. It does not
certify all OMOD combinations, camera/controller paths, mine detonation behavior,
or non-cooperating scripted producers. See the [runtime guide](../tests/runtime/README.md).

## Remaining acceptance before replacing Basic

- Existing world aggressor at an authored combat encounter and a generated enemy
  structure: normal full-body appearance, title/journal/abilities, normal planet
  departure and return, reload/cold restart, long absence and actual reset behavior.
  The old seeds prove inert surrogates at a city and a natural feature only.
- Non-cooperating third-party attack producers that bypass the tested callbacks.
- Broader unsupported-domain fixtures such as piloting, free-space actors and
  hostile boarding layouts. Landed/orbital on-foot refusal in the Frontier passes.

The [world probe and setup guide](../tests/world-shadows/README.md) are ready. It
stamps an existing enemy once, relies on the production hunt's alias, observes
unload/reattach and live cells, and records actual elapsed UT time and any alias
reset events. It neither suspends AI nor adds another subject alias. A long wait
alone does not certify a reset. Two new disposable combat saves are still required.

The wake-up table remains the next slice after these world gates. The five-slot
library, dropped pack, companion rescue, settings and wider traits follow later.
Before disabling this test pack, call `CancelHunt`, then `ForceRecoverAll`, verify
cleanup and save. Stolen loot remains where it was transferred.
