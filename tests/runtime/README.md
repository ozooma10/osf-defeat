# Runtime acceptance

These are local, disposable MO2 + SFSE tests. Enemy-world persistence remains a
separate [world-probe acceptance task](../world-shadows/README.md).

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
nearby wall impacts. A real healthy-save reload restores the seed and discards
equipment, placed mines, and health changes. This proves the tested keyboard/mouse
attacks, not every OMOD, camera/controller path, or third-party scripted producer.

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
edited. The separate inventory is `tests/runtime/refusal-cases.json`.

The test saves a healthy baseline, moves to the previously captured interior
marker, and verifies the actual home-ship/interior/landed/space predicates. A
disposable eligible aggressor supplies a lethal health hit through the production
hook. Acceptance requires the production `unsupported-domain` result, no theft,
unchanged weapon OMODs, no hunt/traits/time skip/truce, cleared down-state and scene,
Papyrus death evidence, and a real automatic reload with working controls and
empty Defeat state. It repeats after the actual home ship's `TakeOff()` transition
and requires both `IsLanded=False` and `IsInSpaceEx(False)=True` before the hit.

The native CommonLib dead-bit helper stayed false during the observed player death
transition. The oracle therefore retains Papyrus `OnDying`/`OnDeath` and `IsDead`
observations and requires subsequent load events/epoch changes. The narrow finding
is recorded in OSF RE's `gameplay.defeat_damage_hook` module; no replacement native
offset or general NPC death predicate is claimed.

Coverage is on-foot refusal inside the tested Frontier while landed and in orbit.
It does not certify piloting, free-space actors, hostile boarding layouts, or a
normal UI takeoff route. Those are distinct fixtures, as are the pending authored
and generated enemy-world saves.
