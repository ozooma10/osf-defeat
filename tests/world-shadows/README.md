# Existing-enemy Shadows acceptance

This is the remaining world acceptance gate, separate from the quiet-cell suite.
The old persistence seeds cover New Atlantis and an outdoor natural feature;
neither proves an existing enemy at a generated enemy structure.

Make two disposable manual saves before fighting: one at an authored combat site,
one at a generated enemy structure. Leave an ordinary non-quest enemy alive and
the home ship landed nearby. Keep the original saves and sidecars unchanged.

## Build and prepare

Build the production native DLL without deployment (`tools/test-smoke.ps1` does
this), then build the test-only world variant:

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
Actor Value (`xx000901`) to the normal pack. It is never built by default. Determine
the actual load prefix with `help OSFDefeatShadowsWorldProbe 4 QUST`.

## Run each site

1. Select the existing enemy in the console and record its reference ID and site.
   Start the probe quest with `startquest xx000900`, then run
   `cqf xx000900 BeginTrial <enemy-ref-id> authored` (or `generated`).
   This attributes a **scripted defeat** to that existing enemy and exercises the
   normal consequence. It does not claim a naturally lethal hit. An ineligible
   enemy is refused; do not remove its quest/protection metadata to force a pass.
2. Close the console until recovery finishes, then `cqf xx000900 Inspect`.
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

Papyrus user logs are named `OSFDefeatShadowsWorld` under the game's Documents log
directory. Retain all rotations after each session, the build/fixture manifests,
paired saves/sidecars, screenshots, and a short route/reset account.

The probe writes its identity stamp once. It does not add another subject alias,
disable AI, retain the generated origin Cell, recreate the enemy, or restamp on
reload. Its saved script state retains only scalar identity, base Form and planet.
Production hunt aliases provide persistence. `Inspect` never counts as a planet
trip or visual pass by itself.

`cqf xx000900 Cleanup` cancels the hunt and asks the framework to restore state;
it does not delete the existing enemy or stolen possessions. Discard the private
saves afterward. Missing combat seeds, natural reset behavior, third-party attack
producers and unsupported ship/space domains remain separate acceptance limits.
