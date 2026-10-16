# Core experience slice 0: persistence spike

Status: version 2 corrects the identity oracle, cell lookup, and alias text storage;
fresh-profile runtime acceptance is pending. The [first run's findings](RESULTS-2026-10-16.md)
remain historical evidence. This is a separate development plugin, never part of `pack/`
or `dist/`. It does not register a consequence or require the Defeat DLL.

The ordinary 42-case runtime suite uses a quiet cell. Its save/load passes cannot answer
the generated-site question in [CORE-EXPERIENCE.md](../../docs/CORE-EXPERIENCE.md).
This probe gives that experiment identifiable objects, repeatable checks, and a log.

## 1. Make two clean seed saves

Use your normal MO2 profile and an existing character that can travel between planets.
Do this **before enabling the probe**. Use a new manual save slot for each seed, keeping
your normal saves.

1. **Authored:** land at New Atlantis, leave your ship, and stand outside near the landing
   pad on a flat surface. Wait until combat/dialogue/landing animations finish. Make a new
   manual save through the Save menu. Note its time and location.
2. **Generated:** choose a new planetary landing spot. Walk to a generated structure such
   as an abandoned mine or outpost. End any immediate combat, stand outside on flat ground
   at that site, and make another manual save. Your home ship must still be landed on this
   planet. Note the planet, the landing-site icon, the structure, and the save time.

Prefer a quiet location without an active quest scene. Keep the landing-site icon: return
through that exact icon later; choosing a new point on the surface is a different experiment.
The initial seeds need no live enemy; the probe creates its own disposable subject.

After saving, quit Starfield and close MO2. Find the matching `.sfs` files in the profile's
save folder (normally Documents/My Games/Starfield/Saves). The preparation command copies
each explicitly chosen save and matching sidecars; it never moves or edits the originals.

## 2. Build and prepare a private profile

Prerequisites: CK Papyrus compiler, sibling `PapyrusSource`, Python, the sibling OSF Test
Harness's common helpers/configuration, and Spriggit 0.40.1 with a **.NET 9 SDK/runtime**.
The translator targets `net9.0`; .NET 8's misleading “DotnetToolSettings.xml not found” error
does not mean the package lacks that file. Official tool downloads:
[Spriggit 0.40.1](https://github.com/Mutagen-Modding/Spriggit/releases/tag/0.40.1) and
[Microsoft's .NET install script](https://learn.microsoft.com/dotnet/core/tools/dotnet-install-script).
The builder accepts `-Spriggit` and `-DotNetDirectory`, and discovers local copies under
`build/tools/spriggit-0.40.1` and `build/tools/dotnet9`. It does not install tools itself.

From the repository root:

```powershell
pwsh -NoProfile -File tools/build-persistence-spike.ps1
```

The printed `build.json` path identifies this exact build. The build compiles Papyrus,
creates the ESM, round-trips it through Spriggit's correctness check, and independently
checks the binary record/alias/objective/name inventory. Its receipt contains source and
payload hashes and `runtimeVerified: false`. Generated ESM/PEX files remain in `build/`.

Prepare one seed at a time using that receipt and the exact save path:

```powershell
pwsh -NoProfile -File tools/prepare-persistence-spike.ps1 `
  -FixtureSave 'C:\path\to\authored-seed.sfs' -Kind authored `
  -BuildReceipt 'C:\path\to\build.json' -Plan

# Same command without -Plan creates the isolated profile.
pwsh -NoProfile -File tools/prepare-persistence-spike.ps1 `
  -FixtureSave 'C:\path\to\authored-seed.sfs' -Kind authored `
  -BuildReceipt 'C:\path\to\build.json'
```

Repeat with `-Kind generated` for the second seed. Each invocation creates a fresh named
MO2 profile, payload/output mods, and physical save directory. Ordinary mods from the
harness-configured source profile remain enabled; test runners and Auto Load are disabled.
Original profiles and saves are unchanged. The command checks payload hashes and records
seed hashes in `fixture.json`. It neither launches the game nor reports runtime success.

Select the printed profile in MO2 and launch **Starfield (SFSE)** normally. Use **Load** to
choose the copied seed. Confirm the location before running any probe command.

## 3. Plant the fixture and check save/load

Open the console (`~` on a US keyboard):

```text
help OSFDefeat 4 QUST
```

Use the numeric quest FormID printed by `help` in every `startquest` and `cqf`
command below. The prepared profiles currently resolve it to `04000800`, but the
load-order prefix can change. The EditorID did not resolve in the runtime console.

```text
startquest 04000800
cqf 04000800 Begin authored
```

Use `Begin generated` in the generated seed. Close the console and let the script run.
An inert, ghosted Ecliptic and a small ammo container appear beside you. Neither call to
`PlaceAtMe` uses forced persistence. The running quest holds them in optional aliases.
The actor contains **23 credits**, the pack **137 credits**. Don't take either inventory
until the persistence checks are complete.

Version 2 gives each created reference a distinct random identity stamp in a private
Actor Value. Only creation writes the stamp; inspection and loading never refresh it.
The oracle checks both the original base and stamp. Baseline negative controls verify
that new, unstamped and wrongly stamped instances of the same bases are rejected.
Raw FormIDs remain in the logs for diagnostics and may change after a cold restart.
The aliases remain the only lasting ObjectReference holders. The quest stores its
origin and ship interior as typed Cell forms, which participate in save remapping;
it no longer resolves a dynamic cell through `Game.GetForm(savedInteger)`.
This is an alias-held reference test with saved Cell metadata, not proof that an
entire unreferenced generated site survives. Use only fresh seeds with this version.

Record screenshots of the subject's displayed name (**Persistence Subject**) and the
quest journal. The first objective uses `<Alias=Subject>` and should show that name;
both actor and pack should have working objective targets. These are visual observations,
not automatically certified by `IsObjectiveDisplayed`.

Make a new manual save with the planted fixture. Reload it, return next to the pack if
needed, and run:

```text
cqf 04000800 Inspect reload
```

Keep that save and its sidecars. Also quit to desktop, launch the same private profile,
load the planted save again, and run `Inspect cold-restart`. Do not rerun `Begin` after
loading: recreating the objects would invalidate the experiment.

## 4. Leave and return normally

1. Travel normally to **a different planet and land there**. Do not use `coc`, `moveto`,
   or a freshly created landing site as a substitute for the departure/return.
2. Run `cqf 04000800 MarkAway`. It requires a different non-null planet,
   the saved Cell form to resolve and be detached, and both test references to have
   unloaded 3D. It logs origin attachment/loading and each alias's cell/3D separately,
   and checks the identity stamps away from the site too.
   A `BLOCKED` message is not a pass: retain it and investigate.
3. Make another new manual save there. Reload it (or quit/restart and load it) so the
   aliases must survive a save made while the original site is unloaded.
4. Return using the **original landing-site icon**, then walk back to the original pack.
5. Run `cqf 04000800 CheckReturn`. This checks the original reference
   identity stamps, remapped cell, planet, live/enabled/loaded state, inventories,
   pack position, and objectives. It logs elapsed universal hours since `MarkAway`.
   It refuses a return claim unless the departure was observed. It never respawns or
   moves missing references to manufacture a pass.
6. Record names and quest markers again, then save the returned state in a new slot.

Repeat the trip once after waiting/sleeping for 48 local hours away from the site. Record
the actual elapsed game time; this is a stress observation, **not a claim that every cell
reset deadline was exercised**. If the original landing site disappears or cannot be
re-entered, record that as a design failure even if the alias still resolves remotely.

Test the additional naming seam only after recording the alias-title results:

```text
cqf 04000800 TryOverrideName
```

The script requests **Persistence Override** via `SetOverrideName(Message)`. Record what
the actor and journal actually display, including after save/load. Alias-name precedence
is part of the experiment; a request being accepted is not proof that the visible name changed.

## 5. Probe the landed ship interior

After the planet tests, board your landed home ship normally and stand at a clear floor
position in a hab, away from furniture/doors. Run:

```text
cqf 04000800 RememberShip
```

This captures a marker in a third alias. Exit the ship normally, then run:

```text
cqf 04000800 TestShip
```

The probe checks that the same home ship is landed on the player's planet before calling
`MoveTo`. Arrival must be in the captured interior cell, in the correct ship, and near the
marker. Check movement, camera, and that the exit door works. Keep before/after saves and
screenshots. Repeat after a save/load with the marker captured.

This proves movement **to a known interior marker** only. Automatic discovery of a safe
interior destination, different ship layouts, docking, orbit, and boarding combat remain
outside this test. Do not enable ship-domain defeat based on this result.

## Evidence and decision gate

`Debug.TraceUser` writes `OSFDefeatPersistenceSpike` under Starfield's Papyrus user logs
(normally Documents/My Games/Starfield/Logs/Script/User). Copy all matching log rotations
after **each session**, before another run can rotate them away. Retain the build receipt,
`fixture.json`, planted/away/returned saves and sidecars, and screenshots together.
Log lines distinguish `PASS`, `FAIL`, `BLOCKED`, and `VISUAL-REQUIRED`; an inspection pass
does not certify a planet trip, a cold restart, or visual results by itself.

| Observation | Authored | Generated |
| --- | --- | --- |
| Same actor/pack and inventories after reload | 10/10 assertions pass | 10/10 assertions pass |
| Same actor/pack after cold process restart | 2 raw-ID assertions fail | 3 raw-ID/cell assertions fail |
| Departure observed with original cell detached | Blocked on Gagarin, including reload | Not run; cell-ID check needs repair |
| Original site re-entered after away-save/reload | Not completed | Not completed |
| Repeated trip after recorded wait | Not run | Not run |
| Alias title and objective text/targets | Journal `[...]`; actor name pending | Journal `[...]`; actor name pending |
| Message override behavior after reload | Not run | Journal still `[...]`; actor name pending |
| Landed ship MoveTo and controls/exit | Pass before/after reload | Pass before/after reload |

See the [results and evidence index](RESULTS-2026-10-16.md) for limits and next work.
Raw dynamic IDs changed after restart while inventories and pack position survived;
these assertion failures alone do not establish lost objects or a lost generated site.

The actor is a fresh non-forced-persistent surrogate with AI disabled to prevent roaming;
this does not yet certify promoting an encounter's existing actor or surviving an actual
cell reset. The pack tests alias-held container/inventory persistence, not weapon instance
mods. Record these limits with any positive result. If a generated site is lost, keep
slice 1 gated on a design that does not strand stolen items or an incurable debuff there.

When both location runs answer the gate, implement **Minimal Shadows** next: one nemesis,
one title/buff/debuff/reward, weapon and credit transfer, blackout, hunt objective, and the
post-recovery truce. Expand to five slots only after that complete loop works.

`cqf 04000800 Cleanup` removes only references matching their original base and
identity stamp, including after ID remapping; a mismatch is refused and logged.
Retain evidence before cleanup, discard test saves afterward, and switch back to the
normal MO2 profile.
Keep the clean seeds unchanged for reruns.

## Offline setup checks

```powershell
pwsh -NoProfile -File tests/persistence-spike/preparation-tests.ps1 `
  -BuildReceipt 'C:\path\to\build.json'
```

These 17 checks run the real preparation command against a private fake MO2/save tree.
They cover copy integrity, source-profile preservation, mod/plugin lines, isolated save
paths, effective game settings, logging, read-only planning, and rejection of a changed build payload. They do not
launch Starfield or change the existing smoke-suite inventory.
