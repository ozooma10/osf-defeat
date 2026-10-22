# Existing-enemy Shadows acceptance

This is the remaining world acceptance gate, separate from the quiet-cell suite.
The old persistence seeds cover New Atlantis and an outdoor natural feature;
neither proves an existing enemy at a generated enemy structure.

Make two disposable saves before fighting: one at an authored combat site,
one at a generated enemy structure. Leave an ordinary non-quest enemy alive and
the home ship landed nearby. Keep the original saves and sidecars unchanged.

## Fast iteration

Keep a pre-combat checkpoint, a recovered active-hunt checkpoint, and an away
checkpoint for each site. Reuse the relevant checkpoint for focused failures;
do not repeat a successful approach or planet route for every assertion. During
fixture preparation only, native commands may position the player at a verified
existing site marker. Record that setup shortcut. Do not spawn/alter the enemy,
count setup movement as travel acceptance, or move the nemesis to make it pass.

`tools/world-checkpoint.ps1` uses the existing owned world session's `control.json`
and the harness native command channel. It sends no desktop input and requires
no foreground focus. For example, after recovery at a live hunt:

```powershell
pwsh -File tools/world-checkpoint.ps1 -ControlFile $control -Action ReloadCheck -Name osfworld_hunt_reload_01
pwsh -File tools/world-checkpoint.ps1 -ControlFile $control -Action Probe -Observation Away
pwsh -File tools/world-checkpoint.ps1 -ControlFile $control -Action ReloadCheck -Observation Away -Name osfworld_away_reload_01
```

`Save -Name osfworld_setup_01` creates a checkpoint without requiring a hunt.
`Restore -Checkpoint <checkpoint.json>` verifies the same-session save hashes,
observes fresh native revert/load callbacks and loaded cell, and records timing.
Use `prepare-shadows-world.ps1` with the retained `.sfs` for a cold start. A
successful save/load operation alone is not a world acceptance pass. `ReloadCheck`
also requires the selected probe assertions before and after the reload.

Close pause/dialog menus before reloads. A modal during reload is reported as
blocked, without dismissing it or retrying the mutation. Read the dialog and
verify the current state before issuing a new operation. Checkpoint results,
paired saves/sidecars, hashes, and fresh probe output are retained beneath the
session's `checkpoints/` directory. Prefer milestone/failure screenshots over
capturing every movement; continuous 4K captures can consume substantial storage.

Keep normal departure/return and independent natural-reset evidence as separate
acceptance gates. Background probe results do not certify title/body rendering,
input behavior, a normal site route, or an actual reset.

## Build and prepare

Build the production native DLL without deployment (`tools/test-smoke.ps1` does
this). Build OSF Animation with its `tools/BuildProduction.ps1` so the private
profile also receives the blackout-reload input-lock fix (`fcf9714` or later).
The helper copies both built DLLs and records their hashes; the enabled Animation
mod continues supplying its assets. It also stages Defeat's cower scene manifest,
framework overrides and settings so disabling the installed Defeat mod does not
remove its presentation assets. Then build the test-only world variant:

```powershell
pwsh -File tools/build-shadows.ps1 -WorldProbe
pwsh -File tools/prepare-shadows-world.ps1 -FixtureSave 'C:\path\save.sfs' -Kind authored -BuildReceipt 'C:\path\build.json' -Plan
pwsh -File tools/prepare-shadows-world.ps1 -FixtureSave 'C:\path\save.sfs' -Kind authored -BuildReceipt 'C:\path\build.json'
```

Repeat preparation with `-Kind generated` and its seed. The helper creates fresh
owned MO2 payload/output/profile/save directories, verifies copies, disables
conflicting test/Defeat payloads, and stages the production DLL/API/settings.
It does not launch the game or change the Default profile or shared overwrite.
Close MO2 and Starfield before preparation; launch the returned profile locally
through MO2 + SFSE and manually load its private seed.

The world variant adds only a diagnostic quest (`xx000900`) and a private identity
Actor Value (`xx000901`) to the normal pack. It is never built by default. Send
`cgf "OSFDefeatShadowsWorldProbe.StartProbe"` through the native command channel.
It resolves the form by plugin filename, starts the diagnostic quest and logs its
actual numeric ID and running state. Convert the logged decimal ID to hex for
the `cqf` commands below; do not assume an EditorID lookup or load-order prefix.

## Run each site

Dispatch setup commands through the Test Harness native console channel. The
test-only `ListCombatTargets` function logs the IDs and eligibility of the
player's existing combatants; it does not spawn, retain or alter them.
Keyboard/mouse travel requires explicit
interactive opt-in under the harness's current instructions.

1. Use `ListCombatTargets` through the native command channel to record an
   existing enemy's reference ID, eligibility and site.
   After `StartProbe` reports a running quest, run
   `cqf xx000900 BeginTrial authored` (or `generated`).
   This arms observation only. Let a normal lethal attack enter the production
   player consequence; retain the native aggressor/consequence log. The actor
   defeat callback checks and stamps the actual existing aggressor before the
   aftermath's delayed promotion. This avoids guessing which combatant will land
   the lethal hit. `DefeatActor` only enters the low-level down state and does not
   dispatch that consequence. An ineligible enemy is refused; do not remove its
   quest/protection metadata to force a pass. A missing/late callback or different
   promoted enemy must fail identity; restore the untouched pre-combat seed.
2. Let recovery finish, then `cqf xx000900 Inspect`.
   Capture full-body appearance, displayed title, journal target and both abilities.
   Save/reload, inspect, then cold restart and inspect again.
3. Leave normally for another planet. `cqf xx000900 Away` must observe the alias
   detaching and the actor unloading, then save/reload **away**. Return by the
   original landing/site route and run `cqf xx000900 Returned`. Do not teleport or
   respawn the subject to manufacture access.
4. Repeat after a longer absence. Record UT hours from the probe rather than
   claiming local sleep hours equal UT hours. A long wait alone does **not** prove
   a cell reset. Retain separate evidence of an actual reset (e.g. normal ambient
   encounter/container reset); the probe logs any nemesis alias `OnReset` event
   without assuming that one must fire for a persistent actor.
5. Kill the nemesis normally, retrieve the weapon and credits, and verify the
   debuff clears and the reward follows the killer/level rule. Record failure if
   the original site disappears or becomes inaccessible, even if the alias resolves.

Before combat, `cqf xx000900 WatchDeath` arms alias death-event observations and
records the current runtime ID and stolen weapon base. `StartProbe` also calls
this for a saved active hunt. After death use `ObserveResolution`, then
`ObserveResolution True` after normal looting. It compares the death callback
with `Actor.GetKiller`, and checks cleanup, objective, reward and weapon ownership.
These diagnostics retain no additional actor reference. Re-arm after every load;
runtime IDs can remap. A missing identity or death event cannot certify resolution.
Companion combat may need separate attribution evidence: a reward notification
alone does not prove a direct player kill.
Observe resolution promptly: another natural defeat can start a new hunt. In
that case shared cleanup/reward checks are blocked, while original-corpse loot
counts remain observable. Death callbacks are disarmed after the first death.
Repeated `Away` observations preserve the first departure time for that trip,
including across reloads; they do not restart the absence clock.

Papyrus user logs are named `OSFDefeatShadowsWorld` under the game's Documents log
directory. Retain all rotations after each session, the build/fixture manifests,
paired saves/sidecars, screenshots, and a short route/reset account.

The probe writes its identity stamp once. It does not add another subject alias,
disable AI, retain the generated origin Cell, recreate the enemy, or restamp on
reload. Its saved script state retains only scalar identity, base Form and planet.
Production hunt aliases provide persistence. `Inspect` never counts as a planet
trip or visual pass by itself. Its loaded-near-player check requires loaded 3D,
the original planet and at most 100 game units of separation. Parent-cell
equality was disproven as a proximity oracle by the generated robot: it remained
loaded 25-36 units away with different parent cells. Both cell IDs and distance
remain logged; the earlier failed equality checks are retained, and visual/site
route evidence is still required independently.

`cqf xx000900 Cleanup` cancels the hunt and asks the framework to restore state;
it does not delete the existing enemy or stolen possessions. Discard the private
saves afterward. Missing combat seeds, natural reset behavior, third-party attack
producers and unsupported ship/space domains remain separate acceptance limits.

## Focused theft diagnostics

`tools/build-shadows.ps1 -WorldProbe -TheftDiagnostic On` adds retained build-copy
logging around the theft check and transfer. `Off` returns before weapon theft,
after credit theft; status 8 identifies that test-only bypass. Neither mode changes
production source or the normal build. Inspect `OSFDefeatShadowsTheft` logs and
the independent world/recovery observations. A bypass pass does not certify loot
recovery or world acceptance. The pre-fix On/Off builds and crash comparison are
retained in `build/theft-20261021-ab.json`.
