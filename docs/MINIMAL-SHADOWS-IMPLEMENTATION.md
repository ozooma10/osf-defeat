# Minimal Shadows implementation

Slice 1 of [CORE-EXPERIENCE.md](CORE-EXPERIENCE.md), implemented as a separate
[staged pack](../shadows/README.md). Shadows has its own release track; Basic Consequences
remains the optional starter pack. See the [roadmap](../ROADMAP.md) for product boundaries.
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

## CommonLibSF revalidation (2026-10-21)

The update to CommonLib `4242565` exposed two regressions. Its weapon-fire event
source ID is zero, which aborted startup, and its no-argument `Actor::IsDead()`
again reads the cached bit that stayed false during actual deaths. Defeat now
keeps the previously proven event binding (`822934`, with the existing address,
vtable and instruction checks) and Papyrus-equivalent death predicate local.
The dependency checkout remains unchanged.

With those local fixes on Defeat `6d12b18`, validation passes:

- Full regression: **78/78 live assertions**, **85 native cases / 1,036 checks**,
  **136 runner checks**, payload/build and source integrity. Receipt:
  `build/commonlib-20261021-full-04/summary.json`; artifacts:
  `OSF Test Harness/artifacts/20261021-165252-298-DefeatSmoke`.
- Scripted attacks: **23/23**, including player and NPC native weapon fire.
  Receipt: `build/commonlib-20261021-scripted-03.json`; artifacts:
  `OSF Test Harness/artifacts/20261021-170158-050-DefeatSmoke`.
- Death and landed/orbital refusal: **12/12**. Receipt:
  `build/commonlib-20261021-death-01.json`; artifacts:
  `OSF Test Harness/artifacts/20261021-164947-594-DefeatSmoke`.
  All three dead checkpoints retain `commonlibIsDead=false`, while Defeat and
  Papyrus both report true and the production entry points reject the actor.

All three receipts share source hash
`1477C5B3F4B1846AAA0CECE03C1719E106D0BBC77D69A26EE594FB67332D418D`.
The full run uses four distinct owned processes; their identities and launch
tokens are retained in `runtime-sessions.json`. Original fixtures and live
Default payloads were not changed. Runtime entrypoints now require explicit
`-Interactive`; save/load setup uses the harness native command channel, while
the private profile keeps the game active and restores owned foreground after load.

Earlier failed/blocked evidence remains retained: full-01 hit the zero-ID startup
abort; full-02 and full-03 lost foreground (full-03 completed 60 checks);
scripted-01 timed out waiting for foreground gameplay; scripted-02 completed all
23 gameplay checks but failed source integrity when concurrent documentation
edits landed. These attempts are not counted as passing runs. World acceptance
is separate from the successful quiet-cell regression.

The first world session loaded the original New Atlantis seed in a fresh owned
profile and captured the player with the landed home ship. Foreground ownership
then repeatedly changed to Edge, so travel/combat input was held and the owned
game stopped. Retained result: `build/commonlib-20261021/world-runtime/20261021-170442-037-ShadowsWorld-authored`.
No enemy-world gate passed in that session. Native EditorID quest setup produced
no probe log; the test-only `StartProbe` bootstrap now resolves the quest by
plugin filename and logs its actual ID. `build/commonlib-20261021-world-probe-03`
passes Papyrus compilation and the 11-record round trip. The bootstrap was then
verified live: quest `07000900` resolved and ran. Normal travel from New Atlantis
reached Kreet Research Lab. Its cafeteria pirates (`0005236A` through `0005236C`)
and the Lab 01 pirate (`000543C3`) were all rejected because their leveled bases
are unique; the other recorded eligibility gates passed. They were not modified
to bypass the restriction. Route and diagnostic evidence is retained in world
sessions `20261021-171036-540-ShadowsWorld-authored` and
`20261021-174432-490-ShadowsWorld-authored`.

That work also exposed missing presentation assets in the private setup: it
disabled the installed Defeat mod without staging `OSF/osfdefeat.osf.json`.
`prepare-shadows-world.ps1` now copies and verifies that scene manifest and the
framework overrides. The corrected profile showed a real `osf.defeat.cower`
scene start, recovery and restored movement during natural combat. This is setup
verification, not an existing-enemy persistence pass.

The generated route walked north from the preserved Kreet landing save to the
scanner-identified Abandoned Weapon Station. Session
`20261021-175213-890-ShadowsWorld-generated` preserved approach, perimeter and
pre-combat saves with both sidecars. Existing enemy `FF028BF8` (base `00375AA4`,
level 5 against player level 2) passed eligibility. The first trial exposed a
probe error: the low-level `DefeatActor` verb does not select/launch the player
consequence. That attempt is retained as blocked, with no persistence pass.
Probe build `commonlib-20261021-world-probe-05` compiled and round-tripped 11
records, but its preselected enemy differed from the actual lethal attacker in
session `20261021-181040-728-ShadowsWorld-generated`. Native logs show normal
Shadows consequence dispatch and recovery; `Inspect` correctly failed identity.
The post-defeat save is retained, without a persistence claim.

Build `commonlib-20261021-world-probe-06` also compiles and round-trips 11 records.
`BeginTrial generated` now arms the actor-defeated callback before combat. That
callback checks the actual lethal aggressor's eligibility, loaded cell, original
planet and Health defeat cause, then stamps it once before delayed promotion.
It retains no additional Actor reference or alias. In session
`20261021-181806-972-ShadowsWorld-generated`, the callback captured existing
hostile Model A robot `FF01D994` (base `00120A54`, stamp `8597501`) at 14:28:53,
immediately after the native lethal-hit/consequence log.

At 14:28:55 Starfield crashed with an access violation at RVA `0642202` and
`RBX=0`. The retained Trainwreck stack crosses the installed
`MakeAliasedRefFromInventory` callback (`2076D40`, return site `207714D`) and
inventory/alias frames `20C34AC` and `20C564C`. This localizes the failure during
the weapon-theft alias path; its root cause remains unproven. The next fresh
perimeter-seed launch (`20261021-184252-473-ShadowsWorld-generated`) exited before
the native handshake. Further launches stopped after these two abnormal exits.
The audit retains the crash/Papyrus/MO2 logs, confirms all 14 payload/seed hash
checks, and finds no duplicate native console request IDs. No game or MO2 process
remained after cleanup. No generated persistence, travel, reset or loot-recovery
gate is marked passed. Resume evidence is recorded in
`build/commonlib-20261021/world-resume.json`; the detailed crash audit is in the
`20261021-181806-972-ShadowsWorld-generated` artifact directory.

## Generated-encounter inventory-alias crash (2026-10-21)

The same retained pre-combat save was replayed with copied-source theft
diagnostics. With weapon theft bypassed after credits, the natural Model A
robot defeat recovered normally. With theft enabled, the last log was immediately
before `MakeAliasedRefFromInventory`; no return log appeared. The repeated crash
was at `Starfield+0642202`, with `RBX=0` and the same inventory-alias stack. This
isolates the crashing operation without claiming an engine root cause. Both
attempts and the original saves remain in `build/theft-20261021-ab.json`.

Production no longer creates that inventory alias. The native
`IsSingleInventoryItemUnreserved` query takes the existing inventory read lock,
requires exactly one copy, and refuses all alias metadata, including ordinary
non-quest reservations. This is a conservative snapshot check, not an atomic
reservation. Papyrus transfers the existing sole instance and records its base
only after verified ownership changes. The legacy alias is retained for cleanup
of older saves. No engine offsets or dependency mirrors were changed for this fix.

The same natural encounter then completed theft and recovery: Eon ownership went
from player 1 / nemesis 0 to player 0 / nemesis 1. The nemesis remained alive,
the hunt objective and abilities were present, and controls/AI were restored.
Artifact `20261021-192240-873-ShadowsWorld-generated` retains the logs, screenshot,
and `osfworld_theft_fixed_20261021` save with both sidecars. Its strict
`loaded-at-player` parent-cell equality failed, so the run does not certify world
acceptance; the next probe adds loaded/distance/cell logging to diagnose it.

Focused regressions pass **24/24 loot**, **40/40 Shadows**, and **18/18 lifecycle**
(`build/theft-20261021-{loot-02,shadows-01,lifecycle-01}.json`). True quest-object
and ordinary aliases both refuse theft and retain their item/alias. Complete
modified-Eon OMOD rows survive transfer, reload, three cold-start checkpoints,
natural drop, retrieval and world pickup. Cancellation, alias loss, saved-hunt
cleanup and restarts also pass. The separate earlier startup exit has not repeated
across these fresh starts.

The generated robot subsequently remained loaded 25-36 units away while its
parent cell differed from the player's. The world probe now requires loaded 3D,
the original planet and distance at most 100 units; both cell IDs remain logged
and the failed equality checks are retained. This corrects the observation only.
Visual appearance and normal site access remain independent requirements.

Artifacts `20261021-194015-720-ShadowsWorld-generated` and
`20261021-200426-721-ShadowsWorld-generated` cover normal departure from the
station to Niira, alias detach/unload, save/reload away, a fresh process loading
that away save, and normal return through Kreet Research Lab and the station's
surface-map marker. Walking into the station passed original identity, loaded
proximity, abilities, objective, restored controls/AI and stolen-Eon ownership.
The return logged 5.478745 UT hours since the final away observation. The
`station-unbroken-body.png` capture shows the full robot and level-6 title.
The clean returned checkpoint and sidecars are retained. No actual cell reset
has yet been demonstrated.

Artifact `20261021-203931-325-ShadowsWorld-generated` resolves the companion-combat
attribution question. At 16:43 the alias death callback and `Actor.GetKiller` both
reported player `0x14`, despite no submitted player attack. At 16:44 the original
hunt, abilities and objective cleared correctly, and the level-6 enemy versus
level-2 player satisfied the reward rule. This proves engine-credited attribution,
not the identity of the direct attacker. At 17:03 normal Transfer/Take All looting
recovered the modified Eon: player 1 / original corpse 0. The retrieved save and
both sidecars are retained with clean native state. Credits remain unexercised
because this fixture started with zero.

Later cleanup checks failed after another natural defeat promoted a new hunt;
those logs remain intact and do not supersede the earlier resolution check. The
probe now blocks shared-state cleanup/reward assertions when a different hunt
occupies the slot and disarms death callbacks after the first event. Loot counts
remain independently observable. Repeated recovery also left the HUD hidden in
this session; reload restored it before successful looting. That presentation
observation is retained, without claiming a diagnosed cause or a fix.

The extended-absence run is retained in artifacts
`20261021-210715-597-ShadowsWorld-generated` and
`20261021-212906-924-ShadowsWorld-generated`. Normal travel to Venus followed by
the ship's Wait UI produced 752.687073 measured UT hours away. A fresh process
loaded the away checkpoint and confirmed the same identity remained unloaded.
Normal travel back through Kreet Research Lab and the station marker passed
return at 756.593506 UT hours: original enemy alive, loaded within 91 units,
original planet, hunt objective/abilities, restored controls/AI, and stolen Eon
ownership. `osfworld_long_return_20261021` and both clean sidecars are retained.
The UI's selected 800-hour estimate is not substituted for the measured clock.
No alias reset event was recorded, and no independent ambient reset baseline was
established, so actual reset remains unverified.

The authored fixture now reaches Mars's Abandoned Mines from a retained
checkpoint. Preparation used `player.moveto 0014B4DD`, an existing exterior marker
verified in the installed ESM; that shortcut is not counted as departure/return
acceptance. The original approach save is retained. A cold start from
`osfworld_authored_quick_setup_20261021` loaded successfully in
`20261021-224216-722-ShadowsWorld-authored`. Four ordinary level-8 enemies passed
eligibility. At 18:46:35, a natural attack from existing Spacer `0014B3EA` began
the consequence (base `000D2148`, stamp `14926544`, Mars `0003F59A`); recovery
completed at 18:46:38. At 18:46:53 the new background checkpoint runner collected
eight passing identity, alive/proximity, planet, abilities/objective, controls,
and AI assertions. Theft status 3 means the Spacer already owned an equal-or-more
valuable weapon, so this encounter does not certify weapon theft. The test added
1,000 player credits before combat, but credit ownership was not independently
asserted. Authored visual, reload, travel, reset, and resolution gates remain open.

`tools/world-checkpoint.ps1` now batches native save/reload/probe operations,
retains exact save/sidecar hashes and timings, and stops on modal dialogs without
automatic input or mutation retries. The initial setup save completed in 0.72s;
the authored eight-assertion probe plus blocked save completed in 1.68s. A final
standalone probe passed all eight assertions in 1.22s at 18:49:36. End-to-end
same-process reload automation is not yet accepted: Starfield repeatedly showed
its low-save-space warning and did not perform the requested load/save. This
persisted after a cold start and with about 10 GB free. The warning was subsequently traced to Steam Cloud quota (see below). Two earlier 60-second timeouts and subsequent immediate
blocked reports are retained. NTFS compression of eight generated Defeat PCH
files freed 2.63 GB with identical before/after SHA-256 hashes; no saves or test
evidence were deleted. Reuse checkpoints and capture milestones rather than
repeating navigation and every movement screenshot.

On the resumed run, Steam's live storage API reported a 1,073,741,824-byte quota
with only 12,219,776 bytes available despite roughly 10 GB free on disk. The 23
owned world-test saves were copied with their sidecars into
`build/commonlib-20261021/cloud-world-backup-20261021/`; all hashes matched.
Steam `FileForget` released only those test saves' Cloud copies, retaining and
verifying the original local/API bytes. Available quota increased to 68,821,840
bytes. Personal saves, other projects and Cloud settings were not changed.
The audit records every file and result. In session
`20261021-231819-354-ShadowsWorld-authored`, a fresh named save then passed in
0.66s and its observed same-process restore passed in 4.14s, with no modal.
This validates checkpoint I/O; active-hunt reload remains a separate assertion.

The same resumed authored session then replayed the natural attack with one
fixture Inflictor (`0026D8A0`) and 1,000 credits. Existing level-8 Spacer
`0014B2BF` (base `0027BB7B`, stamp `1121901`) landed the lethal hit at 19:30:52;
recovery completed at 19:30:55 without a crash. At 19:31:00 theft status 7 and
nine assertions passed: player weapon 0 / nemesis 1, player credits 900 / nemesis
178. The player lost exactly 100 credits; the enemy's pre-existing balance was
not separately captured. The active-hunt checkpoint and sidecars are retained.
The first combined reload completed its native callbacks but the runner rejected
normal Papyrus user-log rotation. It now accepts that bootstrap boundary only
when the previous bytes survive in rotation 1; other reads remain append-only.
The failed report is retained. A fresh combined run at 19:31:49 passed all nine
checks both before and after reload in 6.89s. Visual, cold restart, normal travel,
actual reset and final authored resolution remain separate gates.

A fresh process in `20261021-233249-438-ShadowsWorld-authored` loaded the retained
active hunt and passed all nine assertions at 19:33:59 (1.29s probe). The journal
visibly displayed Hunt Your Nemesis and its The Unbroken/recover possessions
objective. Full nemesis body/title and status-screen visuals remain pending.
An additional defeat occurred during attempted visual framing; the untouched
active-hunt seed was restored with fresh sidecar/native callbacks before travel.
Normal surface-map Fast Travel to Ship reached the Frontier at Cydonia. The
Venus Hills landing then passed all three away checks, including identity,
other planet and unload after the observed detach. Away save/reload passed three
checks before and after in 9.26s. Normal return through Cydonia, the Abandoned
Mines surface marker and walking passed at 20:05:54 after 4.466789 measured UT
hours away: the original enemy was alive, loaded nearby, and still held the
Inflictor and 178 credits. The return checkpoint is retained.

The same session subsequently captured the complete humanoid body and level-8
The Unbroken title (`authored-unbroken-body-title.png`), and the status screen's
Shaken -20 physical resistance (`authored-shaken-status.png`). A focused combat
branch restored the pre-combat visual checkpoint and supplied only the player
with a Magstorm, ammunition and 5,000 health; enemy state was unchanged. This
tests normal weapon death and resolution, not ordinary difficulty balance.
Player gunfire produced OnDying at 20:31:23 and OnDeath at 20:31:26; both callback
and GetKiller credited player 20 (enemy level 8, player level 2). All five
resolution assertions passed at 20:31:58. The dropped Inflictor was visible in
the corpse's normal Transfer menu. Take All recovered it and 178 credits:
at 20:37:01 player weapon 1 / corpse 0, player credits 1,078 / corpse 0, with all
six resolution/retrieval assertions passing. Before looting the balances were
900 / 178. Earlier repeat-defeat attempts remain retained and were not counted.
The loot checkpoint preserves this branch. Authored long absence and independent
actual reset evidence at both sites remain open.

The authored reset branch emptied the ordinary ammo case beside the red tool
chest and gray storage crate in the northern electrical yard, retaining before
and empty quick-loot captures and `osfworld_authored_reset_baseline_01`. The prior
session then ended at its two-hour harness deadline, without a game crash.
Session `20261022-013928-027-ShadowsWorld-authored` cold-loaded that baseline,
passed the nine hunt assertions, and departed normally via the ship and Venus
Rocky Desert. Normal chair waiting selected ten local hours; the report uses
measured UT elapsed time instead of the selector's nominal conversion. Away
reload passed before and after in 10.3 seconds. Normal return through Cydonia and
the original mine marker passed after **957.180664 UT hours** at 22:17:06. The
same original enemy remained alive, loaded nearby and held the Inflictor and
178 credits, with hunt, abilities, controls and AI checks passing.

Walking back to the same ammo case independently witnessed a natural reset:
the previously empty case now contained a frag grenade and two fragmentation
mines (`reset-case-quickloot.png`). Its surroundings and player position match
the retained baseline; no reset command or container/enemy mutation was used.
The nemesis alias logged zero reset events, which is not used as the reset
oracle. All nine hunt checks passed again at 22:33:42, and the paired
`osfworld_authored_actual_reset_01` checkpoint preserves the result. Player-only
fixture health was retained for navigating the hostile yard. Two premature save
requests during ship animations produced no callback; their failed reports are
retained, and the subsequent stable landed save and reload succeeded.

The final generated reset branch, `20261022-033059-458-ShadowsWorld-generated`,
was saved and stopped at the user's request on October 22. A yellow storage crate
under the dismantled ship at approximately (705.8, 1553.1, 31.3) was emptied of
ZeroWire through normal looting and saved as `osfworld_generated_reset_baseline_01`.
Player-only positioning helped establish this fixture before the baseline; it
is not travel evidence. A temporary read-only container survey retained no
references and changed no containers or enemies. Normal walking departure and
Frontier travel to Venus passed all three away observations. The paired
`osfworld_generated_reset_away_paused_01` checkpoint was saved in 0.57 seconds;
all three file hashes were verified before the owned session closed normally.
No long wait or return followed this new baseline, so the generated site's
independent actual-reset witness remains unverified. Source changes and earlier
passing regressions are retained; the normal play profile was not deployed to.

## Remaining acceptance for the separate Shadows release

- Independent natural-reset witness at the generated enemy structure. Authored
  world acceptance is complete; generated identity, visuals, travel, reload,
  long absence and resolution have passed in the retained branches above.
- Custom third-party attack paths outside the scripted compatibility matrix.
  Anonymous `DamageValue` requires explicit `EndRecoveryTruce` integration.
- Broader unsupported-domain fixtures such as piloting, free-space actors and
  hostile boarding layouts. Landed/orbital on-foot refusal in the Frontier passes.

The [world probe and setup guide](../tests/world-shadows/README.md) are ready. It
stamps an existing enemy once, relies on the production hunt's alias, observes
unload/reattach and live cells, and records actual elapsed UT time and any alias
reset events. It neither suspends AI nor adds another subject alias. A long wait
alone does not certify a reset. Both site checkpoints are retained, and an
eligible authored natural aggressor is now verified. The generated theft crash is
resolved in the retained replay. The authored actual-reset witness is now
complete; the generated site's independent reset witness remains pending.

The wake-up table remains the next slice after these world gates. The five-slot
library, dropped pack, companion rescue, settings and wider traits follow later.
Once the blackout has ended, call `CancelHunt`, then `ForceRecoverAll` before
disabling this test pack. Verify cleared aliases, owned abilities and truce before
saving: cancellation can be queued behind an active mutation. Stolen loot remains
where it was transferred.
