# Runtime acceptance

These are local, disposable MO2 + SFSE tests. Enemy-world persistence remains a
separate [world-probe acceptance task](../world-shadows/README.md).

## Run hunt lifecycle coverage

```powershell
pwsh -NoProfile -File tools/test-runtime.ps1 -LifecycleOnly `
  -ResultPath build/new-lifecycle-result.json
```

This uses the disposable quiet-cell seed, spawned actors and uninstrumented
production Shadows scripts. `lifecycle-cases.json` contains 18 assertions for
cancellation, idempotence, alias loss, external stop, stopped admission, restart,
stale target death, restarted hostile-spell subscriptions, reward retention and
explicit reward cleanup. Cleanup must preserve transferred credits and the Eon.

Real checkpoints save/load an active hunt and a cancelled hunt. The saved consumer
must already exist after loading. Clearing the alias before cancellation then
proves that independent cleanup ownership survived the save. Test-owned AI is
resumed before saving; all three retained sidecars require empty framework state.
The final healthy reload restores controls and checks every cleanup registry.
Alias-loss injection does not establish real enemy persistence or cell resets.
This focused mode excludes blackout instrumentation and cold-restart dispatch.

Latest acceptance: **18/18 passed**, Defeat `b5d184a`, receipt
`build/lifecycle-06.json`, artifacts
`OSF Test Harness/artifacts/20261017-212507-872-DefeatSmoke`.
The subsequent full regression passes **78/78 assertions** on Defeat `0141bb6`;
receipt `build/lifecycle-full-05/summary.json`. The separate Shadows regression
passes **40/40**, receipt `build/lifecycle-shadows-07.json`. An earlier lower-level
kill observation failed and did not reproduce in either rerun; the fixture now
records promotion, death, alias, reward and debuff separately, and the runner
reports a recorded negative as an assertion failure rather than a timeout.
See the [implementation record](../../docs/MINIMAL-SHADOWS-IMPLEMENTATION.md#hunt-lifecycle-cleanup-2026-10-17)
for the reproduced failures, fixes and broader regression status.

## Run scripted attack compatibility

```powershell
pwsh -NoProfile -File tools/test-runtime.ps1 -ScriptedOnly `
  -ResultPath build/new-scripted-result.json
```

This uses the disposable quiet-cell seed and uninstrumented production Shadows
scripts. Its separate `scripted-cases.json` inventory contains 23 assertions.
The fixture creates a restrained, AI-enabled NPC and a marker source. Suspended
NPC AI prevents active spell effects from updating and cannot prove damage
attribution. Health is raised for these nonlethal checks; a real healthy-save
reload removes the fixture and requires empty framework state and working controls.

The matrix covers attributed native health damage, restoration/non-health controls,
anonymous and cooperating `DamageValue`, incoming damage, direct `Spell.Cast`,
`RemoteCast` from actor and object sources with player/NPC blame, missing blame,
healing and invalid source, hostile/healing `DoCombatSpellApply`, and player/NPC
`Weapon.Fire`. Spell cases require real health changes; fire cases require native
discharge observations, including when no watched graph/Papyrus callback fires.
Remote-cast observations verify exact spell, blame, hostility and caster vtable;
invalid sources must never reach the validated cast. Every case checks truce,
faction ownership/membership, and the unexpired deadline.

These are engine-API compatibility checks, not certification of arbitrary mod
scripts. Anonymous damage cannot establish who attacked; its integration contract
is explicit `EndRecoveryTruce`. See the [Papyrus API](../../docs/papyrus-api.md).

Latest acceptance: **23/23 passed**, Defeat `ca53475`, receipt
`build/scripted-05.json`, artifacts
`OSF Test Harness/artifacts/20261017-155352-773-DefeatSmoke`.
The [implementation record](../../docs/MINIMAL-SHADOWS-IMPLEMENTATION.md#scripted-attack-compatibility-2026-10-17)
retains the fixes, controls and complete regression results.

## Run broader weapon coverage

```powershell
pwsh -NoProfile -File tools/test-runtime.ps1 -WeaponsOnly `
  -ResultPath build/new-weapons-result.json
```

This focused suite uses the existing disposable quiet-cell save and uninstrumented
production Shadows scripts. It creates no target NPC. Its separate inventory is
`weapon-cases.json`: 26 attack cases plus baseline, save, fixture, passive controls,
and healthy-load cleanup. The ordinary full suite retains its existing coverage;
run this matrix separately when changing attack/truce behavior.

Fixtures include Eon, Grendel, Coachman, Breach, Equinox, Va'ruun Inflictor,
Magpulse, Magstorm, Microgun, Bridger, Novablast, Cutter, three melee weapons,
unarmed, gun bash, five grenades, and frag/inferno/cryo/Tesla mines. Vanilla form
IDs/editor IDs come from the installed Starfield.esm WEAP records. Every case
requires stable truce through equipment setup, actual equipment identity for
primary weapons, an observed production fire/melee callback, and truce/faction
cleanup before the deadline. Guns also require ammunition consumption; throwables
require exactly one item consumed. Cutter uses the fire callback because its
recharging energy is not ordinary inventory ammunition. Reload/aim input must
leave truce and ammunition unchanged.

The bounded native graph observer retains callback diagnostics. Explosive cases
wait six additional seconds after inspection; test health is raised to tolerate
nearby wall impacts. Each throwable starts after a real healthy-save reload, as
does the case following the Bridger. This discards delayed damage and placed
mines, with a new load event/epoch and empty framework state required each time.
The final healthy load also restores equipment and health. This proves the tested keyboard/mouse
attacks, not every OMOD, camera/controller path, or third-party scripted producer.

Latest acceptance: **31/31 passed**, Defeat `5a9dcc6`, receipt
`build/scripted-weapons-01.json`, artifacts
`OSF Test Harness/artifacts/20261017-154604-705-DefeatSmoke`. The
[implementation record](../../docs/MINIMAL-SHADOWS-IMPLEMENTATION.md#broader-weapon-acceptance-2026-10-17)
retains scope, process identity, isolation-load evidence, and earlier iterations.

## Run blackout coverage

```powershell
pwsh -NoProfile -File tools/test-runtime.ps1 -ShadowsOnly `
  -ResultPath build/new-shadows-result.json

# Just the post-theft/post-time checkpoints and their cold restarts:
pwsh -NoProfile -File tools/test-runtime.ps1 -BlackoutOnly `
  -ResultPath build/new-blackout-result.json

# Framework, Basic Consequences, and Shadows together:
pwsh -NoProfile -File tools/test-smoke.ps1 -RunGame
```

Use a new result path each time. The default disposable quiet-cell fixture is
configured in the sibling Test Harness. The runner owns the private profile,
payload, save copies, game processes, and launch tokens. It rejects changed
sources, foreign/stale action receipts, reused evidence paths, and bad sidecars.

The blackout scenarios save before promotion, after theft (phase 2), and after
time advancement (phase 3). Each pair is loaded in the same process and then
booted in its own fresh process. Cold starts verify the exact hashed save pair,
Auto Load's chosen seed, and distinct process creation times and launch tokens.
The saved Papyrus consumer must already exist; cold inspection cannot create it.

Phases 2 and 3 are too brief for reliable console saves. Only the runtime test
build recompiles an instrumented aftermath: two bounded-by-the-production-watchdog
wait loops pause at generation guards. The ordinary Shadows builder and refusal
suite use the uninstrumented script. `BlackoutTestGates.ps1` refuses changed
anchors; the runner contract tests verify that removing the loops reproduces the
entire production source exactly. The gate stays closed during reload: the real
load callback must invalidate the saved stack and recover the player.

The post-theft checkpoint proves one 100-credit transfer from a 1,000-credit
baseline, the actual nemesis alias/objective/traits, and transfer of the modified
Eon before the time skip. The post-time checkpoint also requires 2–3.1 elapsed
game hours. Reload checks sample twice, three seconds apart: no duplicate theft
or time skip, working controls, stopped aftermath, retained hunt, and the same
complete sorted OMOD rows. Cold OMOD comparisons use the first process's recorded
pre-theft baseline; saved actor, phase, credits, and time come from Papyrus state.
Independent sidecar inspection requires one defeat and one AI suspension
(`DFT0=1`, `PCF0=1`), with valid framing, versions, and CRCs.

This is deterministic save/load fault injection at transaction boundaries. It
does not claim a user can reliably hit those brief phases with normal-timing saves.

## Run ship and orbital refusal

Use a retained slice-0 save whose `ShipMarker` alias is inside the current landed
home ship, on the player's planet, plus its matching verified probe build:

```powershell
pwsh -NoProfile -File tools/test-runtime.ps1 `
  -RefusalFixture 'C:\path\to\retained-ship-marker-save.sfs' `
  -PersistenceBuildReceipt 'C:\path\to\persistence-build\build.json' `
  -ResultPath build/new-refusal-result.json
```

Both arguments are required. Payload hashes and conflicts are checked before the
probe is copied into the private test mod. Original saves and profiles are not
edited. The separate inventory is `tests/runtime/refusal-cases.json` (12 assertions).

The test saves a healthy baseline and first exercises a disposable NPC through
alive, defeated, recovered and killed states. Native and Papyrus death predicates
must agree with each expected state, and the production entry points must reject
the killed actor. The test then moves to the previously captured interior
marker, and verifies the actual home-ship/interior/landed/space predicates. A
disposable eligible aggressor supplies a lethal health hit through the production
hook. Acceptance requires the production `unsupported-domain` result, no theft,
unchanged weapon OMODs, no hunt/traits/time skip/truce, cleared down-state and scene,
Papyrus death evidence, and a real automatic reload with working controls and
empty Defeat state. It repeats after the actual home ship's `TakeOff()` transition
and requires both `IsLanded=False` and `IsInSpaceEx(False)=True` before the hit.

The corrected CommonLib `Actor::IsDead()` helper uses the same virtual predicate
and argument as Papyrus. A version-gated native probe compares both at every NPC
checkpoint and before death, during death, and after reload for each player fixture.
It verifies complete function bytes, primary vtables and compiler-visible field
offsets. `nativeDeadBit` records the original raw flag separately from
`nativeIsDead`; the raw flag stayed false during the proven deaths. The oracle
also retains Papyrus `OnDying`/`OnDeath` observations and requires subsequent load
events/epoch changes. Canonical proof is in OSF RE's `gameplay.defeat_damage_hook`
module. The focused suite and full 78-assertion regression pass on Defeat `d78f1a9`
with CommonLib `d18e0bd` (`build/death-refusal-03.json` and
`build/death-full-01/summary.json`).

Coverage is on-foot refusal inside the tested Frontier while landed and in orbit.
It does not certify piloting, free-space actors, hostile boarding layouts, or a
normal UI takeoff route. Those are distinct fixtures, as are the pending authored
and generated enemy-world saves.
