# Minimal Shadows implementation

Slice 1 of [CORE-EXPERIENCE.md](CORE-EXPERIENCE.md), implemented as a separate
[staged pack](../shadows/README.md). Basic Consequences remains the shipped pack.
The disposable-cell acceptance suite passes; existing-enemy world acceptance
remains open below.

## Implemented loop

- One persistent actual-aggressor alias, titled **The Unbroken**, with a hunt objective.
  Another defeat cannot replace an occupied slot, steal again or remove its debuff.
- The hunt retains its own actor reference for cleanup after aliases disappear.
  Cancellation removes owned abilities, aliases, the objective and truce without
  moving stolen loot. Requests received during a mutation remain queued. External
  shutdown also stops subscriptions/timers and resets the stopped quest's journal;
  restarting re-registers player attack callbacks. Alias loss abandons the hunt
  without awarding a kill or removing an earlier earned reward.
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
with the save-epoch guard. Native observers also cover scripted `Weapon.Fire` and
accepted hostile `Spell.RemoteCast` with explicit blame. Shadows watches player fire,
graph callbacks and hostile `OnSpellCast` for misses and `DoCombatSpellApply`.
Actual frag release emits `WeaponFire`; guessed `ThrowGrenade` callbacks did not fire.
`HitFrame` covers melee/unarmed misses. Anonymous `DamageValue` and custom producers
outside these paths must call `EndRecoveryTruce` explicitly. New defeat and
`ForceRecoverAll` end the truce. Pre-existing faction memberships are preserved;
unresolved actors retain cleanup tracking. `TRC0` v1 saves the deadline and ownership;
`CPT0` retains the exact faction ID. See the [Papyrus API](papyrus-api.md).

Loading a saved blackout exposed an OSF Animation input-layer leak: defeat recovery
completed but movement/fighting stayed disabled. Animation commit `fcf9714` reconciles
only its owned layer after load, retaining any new-world lock. World profile preparation
copies the built Animation DLL privately alongside Defeat so it includes this fix.

## Validation — 2026-10-17

- Hunt lifecycle: **18/18 live assertions**, Defeat `b5d184a`, uninstrumented
  production Shadows scripts. Receipt `build/lifecycle-06.json`; artifacts
  `../OSF Test Harness/artifacts/20261017-212507-872-DefeatSmoke`.
- Shadows regression after lifecycle changes: **40/40 live assertions**, four
  owned local game processes, Defeat `fe6b129`; receipt
  `build/lifecycle-shadows-07.json`; artifacts
  `../OSF Test Harness/artifacts/20261018-022512-325-DefeatSmoke`.
- Full regression after lifecycle changes: **78/78 live assertions**, four owned local MO2/SFSE
  processes, **85 native cases / 1,031 checks**, **132 runner checks**, payload
  validation, production build and source-integrity checks pass. Defeat `0141bb6`,
  Harness `93c05e9`, CommonLib `ebc14f7`, Animation `fcf9714`; receipt
  `build/lifecycle-full-05/summary.json`; artifacts:
  `../OSF Test Harness/artifacts/20261018-025908-209-DefeatSmoke`.
  An earlier lower-level reward assertion failed without detailed observations;
  both the focused and full reruns pass it. Its original cause remains unconfirmed.
- Focused post-theft/post-time suite: **13/13 assertions**, three fresh processes,
  Defeat `aa1c7c9`. Receipt `build/shadows-checkpoints-05.json`; artifacts
  `../OSF Test Harness/artifacts/20261017-133250-510-DefeatSmoke`.
- Uninstrumented ship/space refusal and native death checks: **12/12 assertions**
  in the retained Frontier fixture, Defeat `d78f1a9`. Receipt
  `build/death-refusal-03.json`; artifacts
  `../OSF Test Harness/artifacts/20261017-144240-139-DefeatSmoke`.
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
Defeat registries pass after each load. The CommonLib `Actor::IsDead()` mismatch
is fixed: the no-argument helper now dispatches the engine predicate used by
Papyrus instead of reading the separate dead bit. The expanded suite compares
native and Papyrus results for a disposable NPC alive, defeated, recovered, and
dying, plus the player before death, during refusal death, and after both reloads.
Dead actors are rejected by the production eligibility and defeat entry points;
held defeat stays alive. The old bit remains recorded separately and stayed false
even at the landed player's `OnDeath` callback, where the corrected helper was true.
Executable, vtable and field-layout gates pass for all ten explicit comparisons.
Canonical native evidence and the mirrored header fix are in OSF RE's
`gameplay.defeat_damage_hook` module. The suite retains Papyrus death observations
and requires actual automatic load events and epoch changes.
This covers on-foot refusal in that ship, not piloting or hostile boarding layouts.

The earlier `death-refusal-02` run passed the NPC comparisons but stopped on
Papyrus's case-insensitive string interning returning `Landed`; checkpoint labels
now normalize before matching. Its retained failure is not counted as acceptance.
`death-refusal-01` was blocked during preparation by an incorrect build-receipt
path and did not launch the game. The final documentation changes follow the
verified `d78f1a9` source; executable/test source is unchanged after validation.

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

## Scripted attack compatibility (2026-10-17)

The focused `-ScriptedOnly` suite passes **23/23 assertions** on Defeat `ca53475`,
CommonLib `ebc14f7`, with uninstrumented production Shadows scripts. Receipt:
`build/scripted-05.json`; artifacts:
`OSF Test Harness/artifacts/20261017-155352-773-DefeatSmoke`. The owned process was
PID 53024, creation FILETIME `134367260571200486`. The final real healthy reload
restores controls and requires empty defeat, AI-suspension, truce and faction state.

Three complementary paths now handle scripted attacks. Native weapon-discharge
notifications end the firing actor's truce even when `Weapon.Fire` emits no watched
Papyrus/graph callback. Shadows watches hostile `OnSpellCast`, including
`DoCombatSpellApply`, while healing leaves the truce active. An observer at the
validated remote-cast call honors explicit hostile-spell blame and forwards the
original spell application unchanged. These native observers verify the installed
game code before registration/patching. Deferred cancellation carries only actor
ID and save epoch, and unit tests reject callbacks from an old save.

The remote-cast failure was real: an actor-source `RemoteCast(NPC, player, NPC)`
dealt 40 damage, but the health write named the NPC source rather than the blamed
player, leaving player truce active. The fix observes explicit blame before that
information is lost. The matrix separately verifies actor and object caster
implementations, exact blame/spell identity, real damage, and captive-faction
cleanup. Native hostile-count reads agree with Papyrus for damage and healing.
NPC blame, missing blame, healing and invalid sources preserve player truce;
invalid sources never reach the accepted native cast.

Other controls cover attributed health damage, restoration, non-health writes,
incoming spells, direct casts, and anonymous/cooperating `DamageValue`. Anonymous
damage has no attacker and intentionally preserves player truce: its producer must
call `EndRecoveryTruce`. Direct-cast coverage proves attributed damage, not every
spell miss or effect type. Custom paths outside these observations retain the
same explicit integration contract. Canonical native findings and retained static
and runtime evidence are in OSF RE's `gameplay.defeat_damage_hook` module; its
CommonLib mirror and sandbox build also pass.

Regression after the fixes: **31/31 weapon assertions**, receipt
`build/scripted-weapons-01.json`, artifacts
`OSF Test Harness/artifacts/20261017-154604-705-DefeatSmoke`, owned PID 41004 /
creation FILETIME `134367255890452129`, eleven healthy loads and final epoch 13.
The expanded fixture only adds object-caster observations and an offset assertion;
the final full regression passes **78/78 runtime assertions** across four owned
processes, **85 native cases / 1,031 assertions**, and **132 runner checks**, plus
payload, production-build and unchanged-source checks. Receipt:
`build/scripted-full-01/summary.json`. No live source changes occurred during a run.

Earlier failed iterations remain retained. Run 01 exposed missed scripted discharge
and a suspended-AI spell fixture; run 02 corrected that fixture and isolated remote
blame; run 03 proved actual health-write attribution. Run 04 passed 21 cases, then
run 05 added and passed the two object-source controls. None of the failed runs is
counted as a suite pass. Basic remains the shipped pack; Shadows remains staged.

## Hunt lifecycle cleanup — 2026-10-17

The quiet-cell lifecycle suite passes **18/18 assertions**, using spawned actors
and real save/load checkpoints. Defeat `b5d184a`, receipt `build/lifecycle-06.json`,
artifacts `OSF Test Harness/artifacts/20261017-212507-872-DefeatSmoke`; owned PID
46512, creation FILETIME `134367459325878084`. The source remained unchanged during
the run, and the harness stopped the owned game afterward.

The original hunt leaked its NPC ability and player wound when the alias was
cleared or the quest stopped. `OnQuestShutdown` receives already-empty aliases,
so the hunt now saves the actor whose ability it owns independently of the alias.
Its timer clears an orphaned hunt, while explicit cancellation and shutdown also
remove the earned reward and recovery truce. Cancellation requests made while
promotion/death handling is busy are retained until that mutation finishes.
Existing active saves can adopt their live alias when the new ownership field is
empty; this does not reconstruct actors from saves whose old alias was already lost.

The engine ignores objective setters on a stopped quest. A retained diagnostic
run proved that force-hide and mark-failed calls leave the objective unchanged,
while `Quest.Reset()` clears it and leaves the quest stopped. Production now resets
only after owned-state cleanup finishes. Explicit cleanup of a completed hunt
also resets completion/failure flags before hiding its objective.

Coverage includes active cancellation with a truce, repeated cancellation, alias
loss, external shutdown, refusal to promote while stopped, a fresh hunt surviving
the old target's death, and an actual hostile-spell callback after restart.
A credited kill's reward survives idle monitoring and explicit cleanup removes it.
An active hunt survives a real load; its saved actor reference still cleans the
NPC after the alias is deliberately cleared. A cancelled hunt remains clean after
another save/load, and another promotion succeeds. Credit and stolen Eon counts
remain unchanged by cleanup. Final healthy reload requires working controls and
empty defeat, AI-suspension, truce and captive-faction registries. Alias clearing
here is fault injection, not evidence of a world-cell reset or enemy persistence.

Earlier iterations are retained: run 01 exposed alias/shutdown leaks and an omitted
checkpoint allowlist entry; run 02 exposed test-owned AI state in a supposedly
clean checkpoint; runs 03–04 isolated stopped journal state. Run 05 established
the reset behavior and restart callback, then was blocked by capture allocation.
These do not count as suite passes. Run 06 passes without diagnostic recovery.

Earlier full-regression attempts `build/lifecycle-full-01` and `-02` pass **85 native
cases / 1,031 assertions**, **132 runner checks**, payload validation and production
build. Their live runs are **blocked**, not passed. The capture helper's new
diagnostics (Harness `e1ef35a`) identify GPU readback allocation with only
16,306,176 bytes of available system commit despite about 7 GB of free physical RAM.
After WizTree closed normally, `build/lifecycle-full-03` passed 55 live assertions
before the lower-level kill's aggregate observation returned false. The player
reward, other-killer cleanup, and exact modified-weapon retrieval observations
passed. The runner incorrectly reported the negative result as a missing-callback
timeout; test-only commit `fe6b129` now distinguishes that recorded failure and
retains separate promotion, death, alias, reward and debuff observations. No
production behavior changed. The failed run is retained at
`OSF Test Harness/artifacts/20261018-021843-957-DefeatSmoke` (PID 56112, creation
FILETIME `134367635627463424`). Its underlying cause is not established.

The focused `-ShadowsOnly` rerun on `fe6b129` passes **40/40 assertions**, including
all five new lower-level observations, same-process blackout loads and three cold
restarts. Receipt `build/lifecycle-shadows-07.json`; artifacts
`OSF Test Harness/artifacts/20261018-022512-325-DefeatSmoke`. The four owned process
identities are retained in `runtime-sessions.json`: 48600 / `134367639456041448`,
29956 / `134367641501819264`, 58012 / `134367642073959329`, and 52796 /
`134367642685168540`. Source-integrity checks pass and the harness stops each
owned game afterward.
This pass does not explain the previous aggregate failure.

The following full attempt, `build/lifecycle-full-04`, passes the same offline
checks but aborts after 31 live assertions. Its PowerShell process exits with
stack overflow (`0xC00000FD`); a simultaneous read-only PowerShell invocation also
fails allocating double-mapped memory. The last snapshot still says `running`,
and no final runtime receipt was written, so neither snapshot nor summary counts
as acceptance. Retained artifacts:
`OSF Test Harness/artifacts/20261018-023229-847-DefeatSmoke`, PID 55748, creation
FILETIME `134367643756387650`. The game is no longer running; final cleanup
did not produce a receipt. `post-abort-recovery/` retains the latest logs, the
aborted run's owner identity and a separate recovery note; the verified stale
owner file was retired without changing the original reports.
Windows reported a 2,349 MiB pagefile and approximately
7 GiB of available system commit after the game exited.

The final full rerun passes **78/78 live assertions**, **85 native cases / 1,031
checks**, **132 runner checks**, payload validation, production build and source
integrity. Defeat `0141bb6` (production hunt unchanged from `b5d184a`), Harness
`93c05e9`, CommonLib `ebc14f7`, Animation `fcf9714`; receipt
`build/lifecycle-full-05/summary.json` and runtime receipt in the same directory.
Artifacts: `OSF Test Harness/artifacts/20261018-025908-209-DefeatSmoke`. The first
session passes 72 assertions; each of three cold restarts adds recovery and cleanup
assertions. `runtime-sessions.json` records distinct owned process identities:
43612 / `134367659741204820`, 45300 / `134367663379247143`, 6008 /
`134367663959289585`, and 22672 / `134367664577930770`. The harness stops each owned
game, and no game remains running after completion.

Before this rerun, Discord had exited and Edge's window had closed; Edge's
background processes remained. Available system commit measured about 19 GiB.
No capture requirements or gameplay assertions were relaxed. All five detailed
lower-level observations pass again, and the modified Eon's 30 OMOD rows match
its pre-theft baseline through reload, retrieval and player drop/pickup. This
successful rerun completes full regression for the lifecycle changes, but it does
not establish the cause of run 03's earlier negative observation. That failure and
the memory-aborted runs remain retained.

## Loot boundary suite (2026-10-17)

`tools/test-runtime.ps1 -LootOnly` adds 22 expected assertions using the existing
quiet-cell seed. Ten credit cases cover zero, insufficient funds, the minimum,
fractional truncation and the cap. Further cases check unarmed/missing/duplicate
weapons, equal/better enemy weapons, empty/existing ammunition and equipped
mining suit/helmet/boost-pack preservation. Component observations retain both
actors' final credit counts, promotion, theft status, ammunition and armor
results. The final healthy-save reload checks aliases, abilities, controls and
native owned registries. Production gameplay is unchanged. Initial offline
validation compiled the native acceptance DLL and all six Papyrus consumers,
passed 132 runner checks, and found all prerequisites in preflight.

The first live attempt (`build/loot-02.json`, artifacts
`OSF Test Harness/artifacts/20261018-032918-867-DefeatSmoke`) passes 15 assertions,
including all ten credit cases and unarmed/missing-weapon refusal. It stops at
duplicate-weapon preparation; that case did not reach promotion. Additional
preparation observations now retain quest/alias state, eligibility, credits,
player ammunition, weapon counts and the enemy's equipped base. The preceding
`build/loot-01.json` is a blocked launch because an unrelated game was running.
Neither attempt is complete acceptance; no production change is inferred.

Run 03 (`build/loot-03.json`, artifacts
`OSF Test Harness/artifacts/20261018-033234-894-DefeatSmoke`) reproduces preparation
failure with all other setup components passing: adding the two Eons supplied
32 additional rounds, raising the player's fixture ammunition from 37 to 69.
The fixture now normalizes ammunition after all weapon additions/equipment and
also verifies the intended player copy count and equipped base before promotion.
The exact 37-round before/after oracle is retained; production is unchanged.

Run 04 (`build/loot-04.json`, artifacts
`OSF Test Harness/artifacts/20261018-033547-152-DefeatSmoke`) passes the duplicate
refusal, then stops at equal-weapon setup: the suspended enemy still reports
the unowned fists base (`0x1F4`) after `EquipItem`. The fixture now resumes its
restrained actor and requests a draw before checking equipment for the value
gate, and after successful-theft attempts before checking stolen equipment.
The equipped-base assertions remain required; target inventory counts are also
retained as preparation diagnostics.

The final run passes **22/22 live assertions**, including duplicate, equal and
better-weapon refusal; a single round supplied to an empty enemy inventory;
existing enemy ammunition unchanged; all 37 player rounds retained; and the
mining suit, helmet and boost pack still owned and equipped after weapon theft.
The real healthy-save reload clears aliases, abilities and native owned state
and restores controls. Receipt: `build/loot-05.json`; artifacts:
`OSF Test Harness/artifacts/20261018-033923-330-DefeatSmoke`. Defeat `75c43ec`,
Harness `93c05e9`, Animation `fcf9714`, CommonLib `ebc14f7`; source hash
`DB69DA3866F6A041395A141B4170AE2968B7A2FBA090241AC9E4A66BE1FA908A`.
The single owned session was PID 16772, creation FILETIME `134367683882228606`.
Source/dependency integrity checks pass, and the harness stopped the owned game.
No game remains running. All three completed unsuccessful runs remain retained.

These changes fix the new fixture's setup, not production gameplay. The prior
78-case full regression remains the latest full run; this focused suite adds
loot boundary coverage without claiming a new full regression. Quest-item
refusal and instance preservation for other weapon families remain separate
coverage gaps, alongside the world gates below.

## Remaining acceptance before replacing Basic

- Existing world aggressor at an authored combat encounter and a generated enemy
  structure: normal full-body appearance, title/journal/abilities, normal planet
  departure and return, reload/cold restart, long absence and actual reset behavior.
  The old seeds prove inert surrogates at a city and a natural feature only.
- Custom third-party attack paths outside the scripted compatibility matrix.
  Anonymous `DamageValue` requires explicit `EndRecoveryTruce` integration.
- Broader unsupported-domain fixtures such as piloting, free-space actors and
  hostile boarding layouts. Landed/orbital on-foot refusal in the Frontier passes.

The [world probe and setup guide](../tests/world-shadows/README.md) are ready. It
stamps an existing enemy once, relies on the production hunt's alias, observes
unload/reattach and live cells, and records actual elapsed UT time and any alias
reset events. It neither suspends AI nor adds another subject alias. A long wait
alone does not certify a reset. Two new disposable combat saves are still required.

The wake-up table remains the next slice after these world gates. The five-slot
library, dropped pack, companion rescue, settings and wider traits follow later.
Once the blackout has ended, call `CancelHunt`, then `ForceRecoverAll` before
disabling this test pack. Verify cleared aliases, owned abilities and truce before
saving: cancellation can be queued behind an active mutation. Stolen loot remains
where it was transferred.
