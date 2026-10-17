# OSF Defeat automated tests

The test bench follows the sibling Settings/UI/Animation pattern: native production-code
tests, payload checks, a production build, and an opt-in fresh game session with retained
JSON results, logs and screenshots. Offline success and game acceptance are separate.

Run from the repository using PowerShell 7.2+, XMake and MSVC:

```powershell
# Inspect the stages and coverage without building or launching.
pwsh -NoProfile -File tools/test-smoke.ps1 -Plan

# Full offline bench; defaults to releasedbg. Never deploys to the live mod.
pwsh -NoProfile -File tools/test-smoke.ps1

# Offline bench plus a fresh local game smoke session.
pwsh -NoProfile -File tools/test-smoke.ps1 -RunGame

# Fast native iteration using the current XMake configuration.
xmake build -y osfdefeat-tests
xmake test -g tests
xmake test 'osfdefeat-persistence-tests/*'

# Check this PC's game prerequisites without launching or writing a profile.
pwsh -NoProfile -File tools/test-runtime.ps1 -Preflight

# Focused staged Minimal Shadows loop (28 assertions, same disposable profile).
pwsh -NoProfile -File tools/test-runtime.ps1 -ShadowsOnly
```

`-Mode debug` selects debug instead of releasedbg. `-Harness 'D:\...\OSF Test Harness'`
selects a different local checkout. `-OutputDirectory` must name a new directory; failed
evidence is never overwritten. The bench writes `build/smoke/<run>/summary.json`,
`report.md`, per-stage logs and per-suite case receipts. Exit codes are **0 passed**,
**1 failed**, **2 blocked**. `runtimeVerified: false` is explicit on offline runs.
Source content (including uncommitted test files) is fingerprinted before and after.
Bench invocations serialize on `build/tests.lock`; game runs also take the shared
harness's `state/runner.lock`.

The [core-experience persistence spike](persistence-spike/README.md) is a separate,
interactive slice-0 test for authored/generated planetary sites, alias names, and landed
ship interiors. `tools/build-persistence-spike.ps1` builds its development-only plugin;
`tools/prepare-persistence-spike.ps1` copies an explicit seed into an isolated profile.
Its offline build/setup checks do not count as planet persistence acceptance or change
the ordinary smoke-suite case inventory. The slice-1
[existing-enemy world probe](world-shadows/README.md) separately requires combat
seeds for an authored encounter and generated enemy structure. Its build and
21 isolated-profile preparation checks do not establish world persistence.

## Native coverage

Nine binaries run 83 named cases. Checks execute in both debug and releasedbg;
they do not use the C `assert` macro. Each test resets registries, clocks, forms, callbacks,
queues and private JSON fixtures. Fixture directories are created uniquely below TEMP.

| Suite | Behavior covered |
| --- | --- |
| registry | Duplicate registration, shared handle lifetime, concurrent insertion/lease reads, save epochs, disabled native queue. |
| config | Defaults, v1/v2/v3 aliases, sorted directory precedence, whole-file rollback, malformed/future files, finite values, reload, eligibility. |
| conditions | Any/all keyword and faction rules, strict form tokens, causes, null actors, invalid/unsupported gates, classification and hostility. |
| consequence | Legacy/v3 normalization, duplicate IDs, fallback, priority tiers, 64-bit weights and roll intervals, overrides, cooldowns, once per cell. |
| state | Idempotent/reentrant recovery, AI ownership, player events, capture/recapture/release, rollback, deferred MoveTo arguments, unresolved cleanup, handoff context. |
| recovery | Invalid times, minimum delay/time scale, legacy timer holds, competing leases, extension/release, automatic recovery, player/NPC watchdog, missing actors. |
| events | Registration, removal, slot generations, all six channels, deferred actor resolution, missing VM, clearing subscriptions. |
| persistence | Real OSFD callback registration, DFT0/CPT0/CAP0/PCF0/TRC0 round trips, v1 migration, every byte truncation, corrupt counts/flags/times, remapping, failed I/O, deferred/stale/inline loads. |
| damage | Actual detour logic: lethal clamp, pending latch, write-before-queue ordering, immunity, eligibility/resist/early-boot passthrough, stale saves, imminent hysteresis, NPC gates and recent attacker filtering. |

The static test target compiles the real `src/Core` implementations. `native/stubs` and
`Doubles.cpp` replace engine objects, native queues, clocks, VM transport, scene calls and
engine state writes. They do not implement the business logic being asserted.
`DamageDriver.cpp` includes the unchanged production translation unit to reach its private
detour without exposing a public production testing API. Real executable addresses, hook
installation, engine memory layouts and actual Papyrus execution require game acceptance.

`coverage.json`, native source files and XMake targets are checked for agreement. Reports
must contain exactly the expected named cases with actual passing assertions; empty,
duplicate, stale, wrong-process and partial results cannot count as a pass.

## Game smoke and local setup

Pull `main` in Defeat, `dev` in Animation, and `master` in the sibling
`OSF Test Harness` (`ozooma10/testing-automation`). Defeat preflight checks for the
Harness launch options that let it require its own payloads and use installed Settings
as a dependency. Older Harness revisions are reported as blocked before preparation.

The Harness must contain its `scripts` helpers and built
`build/bin/OSFTestHarness.dll` and `OSFTestInput.exe`. Its ignored `config.local.psd1`
must identify an explicit disposable `FixtureSave`. The game, MO2 source profile,
`Auto Load` mod, deployed Animation assets, the sibling `OSF Animation` source/build helper,
the CK Papyrus compiler, sibling `PapyrusSource` imports, and `pack/OSFDefeatBasic.esm`
are also required. Preflight lists missing
paths so another PC can be repaired without guessing or fetching private saves.
If the Animation mod has another folder name, set `DefeatAnimationMod` in that local
configuration. The default harness/source paths are resolved from the sibling checkout;
Defeat contains no hardcoded workstation root. The runner builds Animation through its
private production staging helper and overrides its DLL only in the test payload. Pull
Animation's `dev` branch on the other PC: the persistence host
recognizes Starfield's dynamic `SLocalSavePath` setting so sidecars follow private saves.
No live Animation deployment is changed. Test and pack PSC files are compiled privately.

The runtime runner reuses the existing harness process ownership, input, capture,
launch and report functions. It creates the owned
`OSF Defeat Testing` profile, separate test payload/output/harness/Auto Load mods,
and a fresh `OSFDefeatTestingFixture/<run>` save directory. It copies the chosen save
and matching sidecars, verifies their hashes and requires Auto Load to observe exactly
that one save. It enables the selected Animation dependency and disables conflicting
Defeat payloads in the private profile. Shared overwrite conflicts block the run.

An unrelated running game blocks preparation. Idle MO2 is asked to close normally;
an open dialog blocks instead of being force-closed. Only the PID/start time that matches
the native launch-token handshake can receive input or be stopped. Screenshots use the
harness's foreground Windows Graphics Capture helper. Dependency and test DLL hashes,
source identities, fixture identity, snapshots and current logs are retained.

`osfdefeat-runtime-tests` builds a separate DLL with `OSF_DEFEAT_TESTING`. Conditional
instrumentation observes the recovery update, successful persistence save callbacks,
actual queued 3D reattachments, and a test-only Papyrus native for script observations.
Normal `OSF Defeat` builds contain neither the probe nor its configuration reader.
The test DLL has no auto-install action. Its probe stays inert without private configuration
and an unexpired request bound to the exact process and launch token. Every mutation runs
on the existing Main update seam or through the real Papyrus VM. The suite has a
600-second deadline plus bounded runner startup/cleanup waits.

The 66 game assertions include the original nine defeat/recovery checks and these
acceptance scenarios:

- Real console saves and loads, with fresh `.sfs`/`.osf` pairs retained under `saves/`.
  Sidecar framing, CRC, record versions and counts are independently checked. The defeated
  save must contain two DFT0 entries and one each of CPT0/CAP0/PCF0; healthy/clean saves
  must contain no OSFD block. Native persistence callbacks, load events and epoch changes
  must agree with the runner receipts.
- A new persistent NPC clone: capture, player ownership, faction and AI ownership across
  reload, disable/enable 3D reattachment, actual transfer to the player and release.
  Transfer requires a deliberate initial separation, movement to within one engine unit,
  and at least 75% distance reduction; this allows normal actor collision separation.
- Player lease and down-scene restoration, recovery after reload, healthy-save replacement
  while defeated, and a five-second check against stale scene reassertion.
- Real keyboard movement before defeat, movement blocked while down, movement and mouse
  look after recovery, plus Papyrus movement/fighting/looking control-state observations.
- All six callbacks delivered to a real Papyrus consumer, registration after load, successful
  unregister and a three-second no-delivery check after new transitions.
- Unchanged authored Robbed, Picked Clean, Message, Scrapped, Mauled and Arrested definitions,
  enabled one at a time for deterministic selection. Real quest effects check credit/digipick
  transfers, afflictions, recovery and quest stop. Arrested covers its no-crime-faction
  fallback. A tagged disposable marker checks the shared fade/move/recover helper.
- Minimal Shadows: a disposable Spacer aggressor becomes the single nemesis, takes
  credits and the unique equipped Eon, equips it, and applies the authored resistance
  abilities. Blackout advances at least two hours; recovery restores controls with a
  truce while the hostile NPC's AI runs. An occupied slot is preserved. A real truce
  save/load checks TRC0/CPT0/PCF0 and the unchanged deadline. Real missed knife swings
  cancel truce before expiry; a separate short truce expires normally. Player/other
  kill rules, weapon retrieval and clean-load cleanup complete the loop. Exact
  OMOD metadata (including three legendary slots) is compared across transfer,
  reload, retrieval and explicit world drop/pickup. The expanded suite checks
  reference protection overrides and other negative eligibility, a second full
  defeat while occupied, lower-level reward refusal, missed unarmed/Eon/frag attacks,
  cooperating scripted attacks, external aftermath stop and early-blackout reload.
  Natural death drops are accepted only after exact instance-metadata verification.

`-ShadowsOnly` explicitly runs the 24 Shadows cases plus four setup assertions;
its receipt says `scope: shadows`. It does not certify the other 38 full-suite cases.
The runner privately builds `shadows/` through Spriggit and compiles its scripts.
Branch diagnostics in the Papyrus observation array are not individual test verdicts;
the named `cases` array and exact expected inventory determine acceptance.

The damage injection calls the exact five-argument entry documented in `HookTrigger.md`;
do not replace it with CommonLib's overloaded `ModActorValue` virtual call. An initial
probe using that overload selected the wrong ABI slot and crashed; failed runs are retained.
The real load scenario also caught a production bug: BSService disables its queue during
load. Persistence now stores parsed data and applies reversion/restoration at the normal
Main update before recovery, with an epoch guard against superseded loads.

Runner evidence has 97 positive/negative checks, including every byte truncation of a
sidecar fixture, checksums, duplicate clients/records, stale process identity and inventories.

## Acceptance boundaries

The fixture is a quiet disposable cell. Cold process restarts, failed/cancelled loads,
nonpersistent NPC streaming, cross-cell transfers, opt-in hostile NPC lethal combat,
actual jail/sentence transitions, third-party consumers and physical controllers still need
dedicated scenarios. Robot/creature selection uses actual keywords on the humanoid clone;
other skeletons and subjective pose/camera quality remain outside automated certification.
The coverage manifest records those boundaries in every report. See `TESTPLAN.md` for
the broader manual checklist; historical passes do not certify current revisions.

To add coverage, extend an existing `*_tests.cpp` with `TEST(name)` and `CHECK(...)`.
For a new suite, update `tests/xmake.lua` and `coverage.json`. For a game case, update
`runtime/cases.json`, the probe and its coverage mapping together. Never mark an unexecuted
scenario passed or silently reduce the expected inventory to match a partial run.
