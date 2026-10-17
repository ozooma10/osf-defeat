# OSF Defeat — One-Launch Verification Plan

## Automated regression bench (2026-10-16)

Use `pwsh -NoProfile -File tools/test-smoke.ps1` for the native/payload/build checks and
add `-RunGame` for the isolated live smoke session. See [tests/README.md](tests/README.md)
and [tests/coverage.json](tests/coverage.json) for exact coverage and exclusions.
The runtime inventory now contains 42 assertions, including real `.sfs`/`.osf` saves and
loads, captive ownership/faction/AI restoration, NPC 3D rebuilding and transfer/release,
movement and looking controls, all six Papyrus callbacks, all six authored consequence
definitions (arrest fallback only), and the tagged recovery-marker helper. Each run retains
save pairs, sidecar CRC/record inspections, native and Papyrus observations, logs and WGC
captures. Use the current run's `summary.json` and `runtime.json` as the pass/fail evidence;
failed probe runs are retained as well. The tests exposed and fixed Defeat's restore failure
while the engine task queue is disabled and Animation's dynamic save-path lookup.

Remaining scenario gaps after the combat pass below: cold restart, cancelled/failed load,
nonpersistent world NPC streaming, cross-cell captive transfer, real bounty/sentence handling,
physical controllers, and subjective camera/pose quality. The automated pack gates use a
disposable humanoid clone; the separate combat pass adds real robot and creature aggressors,
but does not certify defeat/capture animations on those nonhuman skeletons.

## 0.1 alpha real-combat pass (completed 2026-10-16)

**Result: the scoped combat checks passed.** Real hostile NPCs landed lethal hits on the
deployed production build, selecting all six Basic Consequences. This was controlled combat
with spawned enemies in a disposable test cell, plus an actual custody transition. It does
not replace extended playtesting in natural encounters. No release was committed or tagged.

Run: `build/alpha-0.1/20261016-172554-988-DefeatAlpha/`. The retained `report.md`,
`logs/OSF Defeat.log`, `Papyrus.0.log`, screenshots, `events.jsonl`, `combat-payload.json`,
`combat-results.json`, and `combat-saves/` contain the evidence. Log times below are local EDT. Starfield was
1.16.244.0, using the owned `OSF Defeat Testing` profile through MO2/SFSE. The live Defeat
DLL and Basic ESM were hash-checked against the local production payload. Animation used a
private DLL with the save-path fix; the live Animation mod was not updated by this test.
Player Health was lowered to 10 to shorten fights; lethal damage came from enemy attacks.

| Check | Observed result | Evidence |
| --- | --- | --- |
| Spacer / Picked Clean | Selected at 13:30:59; theft and contusion notifications; natural recovery at 13:31:53 after save/load | `human-fight.png`, `human-console-history.png`, native log |
| Model A robot / Left for Scrap | Selected at 13:32:29; burn notification; recovered at 13:32:37 | `robot-fight.png`, native log |
| Terrormorph / Mauled | Selected at 13:32:55; lacerations and aftermath notification; recovered at 13:33:16 | `creature-fight.png`, native log |
| Va'ruun / Left as a Message | Selected at 13:35:00; message notification; recovered at 13:35:12 | `varuun-view.png`, native log |
| Human / Robbed and Left | The same Va'ruun attacked again; Message was on cooldown, Robbed selected at 13:35:14 and recovered at 13:35:41 | native log |
| UC security / Taken Into Custody | Selected at 13:35:46; recovery and transfer from test cell `002BE3A9` to UC security interior `0025C673` | `security-next.png`, `security-location-console.png`, cell observations |
| Save/load while down | `alphadown` restored one defeat, reasserted the cower scene, and recovered normally; subsequent movement changed player position | `human-down-loaded.png`, `combat-save-inspection.json`, `events.jsonl` |
| Emergency recovery | `ForceRecoverAll` recovered one actor with zero unresolved records at 13:34:37 | `emergency-executed.png`, native log |
| Disabled processing | Real lethal hits passed through at 13:37:54 and 13:37:59, followed by normal death/reload and runtime reversion at 13:38:01 | native log, `passthrough-outcome.png` |
| Opt-in NPC defeat | Sarah's gunfire downed spacer `FF01EA11` at 13:39:20; it recovered at 13:39:40; `GetDead` subsequently returned 0 | native log, `npc-alive-after-recovery.png` |

The saved-down `.osf` passed independent CRC/record inspection with one `DFT0` v2 record.
The recovered `alphaclean.osf` passed with no `OSFD` block. Enemies resumed attacking after
recovery without `tcai`. Repeated Terrormorph defeats selected the default fallback while
Mauled was cooling down; this does not separately prove once-per-location after cooldown
expires. Security's forced faction hostility caused guards at the destination to attack
again; the test restored that relationship and later reloaded the disposable baseline.

Movement, looking, and unarmed attack input responded after recovery. Firearm aim/fire was
not verified: the final captures named `post-recovery-weapon-ready` and `post-recovery-fired`
actually show unarmed actions. NPC lethal damage came from the companion, not the player.
Exact inventory deltas, full bounty/sentence/release accounting, and long-run stability were
not measured. The passthrough screenshots bracket the death/reload; they do not capture a
death screen. The game was stopped after evidence capture and the test-only NPC opt-in
setting was removed. No production code changes were needed.

The fresh offline smoke run also passed: **75 cases / 889 assertions**, 97 runner/evidence
checks, payload validation, and the production build. Its receipt is
`build/smoke/20261016-172814-48d4bf8b9abc4927b14fccc538e1e440/summary.json`.
That invocation skipped its automated runtime stage; the combat evidence above is separate
from the earlier 42-assertion automated game run.

### Repeat procedure

**Payload.** The live MO2 mods `OSF Defeat` and `OSF Defeat - Basic Consequences` were
redeployed on 2026-10-16 from commit `34ea9ee`. The previous July deployment is backed up in
`build/live-backup/`. Use OSF Animation built from `dev` at `c94274d` or later. After the combat
pass, the live Animation DLL and matching PDB were updated on 2026-10-16 to that exact tested
build, replacing the October 15 DLL. Both copied files passed SHA-256 verification. The
backup and deployment receipt are in the Animation repository at
`build/live-backup/20261016-134808-save-path-fix/`.

**Profile.** Enable `OSF Defeat`, `OSF Defeat - Basic Consequences`, `OSFDefeatBasic.esm` and
OSF Animation, then load a throwaway save. Add a settings file such as
`SFSE/OSFDefeat/Settings/zz-explain.json` containing `{"version": 3, "explainDecisions": true}`
so every selection is logged. Turn god mode off and do not fight back.

**Evidence.** Watch `Documents/My Games/Starfield/SFSE/Logs/OSF Defeat.log` for these lines.

```text
[OSFDefeat] Papyrus API bound
[OSFDefeat] OSF persistence registered
[OSFDefeat] registered defeated actor 00000014 (aggressor ...)
[OSFDefeat] selection pool (priority ...) ... -> '<consequence id>'
[OSFDefeat] player consequence '<id>' selected; recovery held for content
[OSFDefeat] recovered actor 00000014
```

After each scenario recovers, run `killall` before spawning the next enemy. Each spawn uses a
level-scaled, aggressive base form from Starfield.esm.

| Scenario | Console setup | Expected consequence and on-screen result |
| --- | --- | --- |
| Human | `player.placeatme 000D2145 1` (LvlSpacer_Heavy_Aggro) | `robbed_and_left` "Robbed while down" notification, or `picked_clean` about one time in four |
| Robot | `player.placeatme 000C6434 1` (LvlRobotModelA_LC107_Combat_Aggro) | `left_for_scrap`, "registers you as neutralized", energy burn |
| Creature | `player.placeatme 002B2293 1` (LvlTerrormorph) | `mauled`, "prowls off", lacerations; a second defeat in the same cell must not select it |
| Va'ruun | `player.placeatme 00073E65 1` (LvlVaruun_Boss_Aggro_NoScript), then shoot it | `left_as_a_message`, concussion, no credits lost |
| Security | `player.placeatme 00246C4B 1` (LvlSecurity_UC), then attack it and resist arrest | `taken_into_custody`, "Security takes you into custody", then a real vanilla jail transition |

If the Va'ruun or security NPC will not fight, `setenemy 0027872A 0001C21C` or
`setenemy 00052ADC 0001C21C` makes its faction hostile to `PlayerFaction`. That change persists
in the save, so use a throwaway one. The creature row also certifies whether Terrormorphs carry
`ActorTypeCreature`; if the log filters `mauled` for the wrong keyword, record the aggressor base form.

Then repeat these against real enemies rather than the quiet test cell:

1. **Recovery feel.** After standing up, move, look, aim and shoot. Enemies that survive must
   resume attacking without `tcai`.
2. **Save/load while down.** During a held consequence run `save alphadown`, then
   `load alphadown`. Expect to stay down and recover normally.
3. **Emergency escape.** While down, run `cgf "OSFDefeat.ForceRecoverAll"`. Expect immediate
   recovery and working controls.
4. **Passthrough.** Run `cgf "OSFDefeat.DisableProcessing" 1` and take a lethal hit. Expect
   vanilla death and a logged passthrough reason.
5. **Opt-in NPC defeat.** Add `"npcAutoDefeat": true` to the explain settings file, reload with
   `cgf "OSFDefeat.ReloadJSON"`, and shoot a spacer. Expect it to drop and recover instead of dying.

Record any row that fails with its log lines, and distinguish completed checks from the
remaining limits above before making a release decision.

## Cleanup regression pass (2026-10-15)

The simplification commits were later covered by the 2026-10-16 automated game run and
deployed to the live mods on the same day. The historical results further below do **not**
validate these revisions.

Before testing, close Starfield and install the newly built DLL/PDB from
`build/windows/x64/releasedbg/` into the enabled mod's `SFSE/Plugins/` folder. Use a separate test
save and launch through the usual MO2/SFSE setup. Keep the game focused and close the console
between steps: queued operations and recovery need gameplay to advance. A native mutator returning
`True` means the request was accepted; check the resulting state before the next dependent action.

1. **Player lethal damage and normal recovery.** Let an enemy land a lethal Health hit
   with god/immortal mode off. Expect one down-state, no death/reload, then recovery according to
   the selected consequence (8 seconds for the bundled plain down/recover consequence; packs can
   hold longer). Movement, combat controls, and camera should return, and cower should stay stopped
   for several seconds after recovery. Repeat to check for duplicate defeat/recovery work.
   While down, these console queries inspect the handoff context:

   ```text
   cgf "OSFDefeat.GetLastPlayerDefeatCause"
   cgf "OSFDefeat.GetLastPlayerConsequenceId"
   cgf "OSFDefeat.GetLastPlayerAggressor"
   ```

   Expect cause `1`, the chosen consequence ID, and the attacking actor for an ordinary enemy hit.
   A scripted `DefeatActor` call does not exercise lethal clamping or player consequence selection.

2. **Selection and cooldowns.** Enable `explainDecisions` in a test settings override and call
   `cgf "OSFDefeat.ReloadJSON"`. With a cooldown-bearing consequence installed, repeat a defeat
   before its cooldown expires: the log should filter it as `cooling down`. After the configured
   interval it may compete again. The logged selection pool must contain only the highest eligible
   priority, in its original order; tied choices remain weighted. A few rolls do not prove the
   statistical distribution. Do not reload a save between these cooldown checks: reversion clears
   session arbitration state.

3. **Save/load while down.** Save during a sufficiently long down-state, reload, then let recovery
   finish. Expect the player to remain defeated after load, the scene to return, and recovery to
   restore controls/camera. Also load an earlier healthy save while defeated and confirm no old
   queued defeat or scene reassertion affects the healthy player.

4. **NPC capture and persistence.** The existing `test/OSFCaptureTest.pex` helper supplies these
   console commands (install it in the test mod's `Scripts/` folder if missing). Start in a fight,
   then run each step separately, returning to gameplay before the next:

   ```text
   cgf "OSFCaptureTest.DownTarget"
   cgf "OSFCaptureTest.CaptureTarget"
   cgf "OSFCaptureTest.ListCaptives"
   ```

   Expect a downed NPC and at least one captive. Save/reload and run `ListCaptives` again; the roster
   must survive. Then run `cgf "OSFCaptureTest.ReleaseAll"`, allow gameplay to advance, and confirm
   recovery and a zero captive count. Capture does not itself verify opt-in NPC lethal interception;
   test that separately with `npcAutoDefeat` enabled and a hostile NPC taking a lethal hit.

5. **Emergency cleanup.** While defeated and/or holding a captive, run
   `cgf "OSFDefeat.ForceRecoverAll"`. After gameplay advances, expect recovery, restored controls,
   and no captives. Wait several seconds to ensure the cower scene does not restart.

6. **Focused API checks, using a registered content/test script.** The console capture helper does
   not exercise event subscriptions or the legacy timer wrapper. On a defeated actor with no
   competing recovery consumer, call `SetRecoveryTimer(actor, 0.0)` and then `-1.0`: each should
   hold recovery (`GetAutomaticRecoverySecondsRemaining` returns `-1`). Calling it with `5.0`
   should start a countdown and recover after about five seconds of unpaused gameplay. For watchdog
   coverage, acquire a short lease with `AcquireDefeatLease(actor, "cleanup-test", 10.0)`, confirm
   the holder, and leave it unreleased: expect force-recovery and a watchdog log. Register actual
   defeated/recovered callbacks and verify delivery, then unregister and verify delivery stops;
   a consequence quest starting by itself does not prove callback delivery.

Native evidence is in `OSF Defeat.log` under the Windows Documents folder's
`My Games/Starfield/SFSE/Logs/`. Look for the recovery-hook install, Papyrus binding, persistence
registration, `registered defeated actor`, `recovered actor`, and consequence filter/selection
messages. Record which steps were actually run and any failing log lines.

## Historical verification and broader pack checklist

> **RESULTS (2026-07-22): green end-to-end except the watchdog (§4, deferred).** Verified in-game:
> deploy/bind (7 consequences after EM removal), Health down + recovery + save/load, all 6 pack
> consequences incl. the first-ever positive `ActorTypeRobot`/`ActorTypeCreature` matches, cooldown
> arbitration + fallback, the full capture loop (down/capture/transfer/persist/release), and the
> `ForceRecoverAll` safe-uninstall + `DisableProcessing` passthrough. **Two bugs found + fixed:**
> (1) camera not restored after the down-scene — OSF Animation `scene_orbit` teardown; (2)
> `TransferCaptive` never moved the captive because `LookupByID(0x14)` returns null for the player
> (now resolved via `PlayerCharacter::GetSingleton()`). **Deferred:** §4 watchdog force-recovery —
> needs a staged never-releasing hold; the mechanism is unit-simple and the deadline path is covered
> by the safe-uninstall test.

Everything below was **implemented and build-verified**; this was the single play session that turned
"compiles" into "works." Each item has a **confirm** line naming what you should see (usually a
`[OSFDefeat]` line in the SFSE log at `Documents\My Games\Starfield\SFSE\Logs\` plus an on-screen effect).

## 0. Deploy (game closed)

- [ ] Close Starfield (a running game locks `OSF Defeat.dll` — deploys will fail "busy").
- [ ] Framework: `xmake build -y` — deploys DLL/PDB, `OSFDefeat.pex`, and the JSON defaults.
- [ ] Pack: `pack\Build.ps1` — compiles + builds `OSFDefeatBasic.esm` + deploys the mod.
- [ ] Test harness: copy `test\OSFCaptureTest.pex` into `MO2\mods\OSF Defeat\Scripts\` (remove after).
- [ ] MO2: enable both mods + `OSFDefeatBasic.esm` in the load order.
- [ ] Boot to a save. **Confirm:** log shows `[OSFDefeat] Papyrus API bound` and `OSF persistence registered`.
- [ ] Optional: set `explainDecisions: true` in `SFSE\OSFDefeat\Settings\default.json` to log the
      selection roll; `npcAutoDefeat: true` if you also want NPCs to auto-down (not required below).

## 1. Core down/recover (no global combat-AI gate)

- [ ] Take a lethal **Health** hit in a fight. **Confirm:** you drop into the cower down-state instead
      of dying; log `registered defeated actor 00000014 (aggressor …)`. (Cause is Health; with
      `explainDecisions` on it also shows the per-consequence filter roll.)
- [ ] Wait out recovery. **Confirm:** you stand up; enemies resume attacking naturally (no freeze, no
      `tcai` gate); log `recovered actor 00000014`.
- [ ] Save while down, reload. **Confirm:** you are still down after load and recovery resumes (DFT0).

> EM is gone (2026-07-21): the player has no EM/stun defeat path. Only Health and scripted downs exist.

## 2. Pack v0.3 — one defeat per aggressor context

For each, get defeated by that aggressor type and watch the beat. `cooldownSeconds` means re-test after
a wait; **Robbed vs Picked Clean** share priority 10 so which fires is a weighted roll (Robbed ~3:1).

- [ ] **Robbed and Left** — human aggressor, Health down. **Confirm:** notification "Robbed while
      down: N credits taken"; the credits are in the aggressor's inventory; ~35% a minor sprain.
- [ ] **Taken Into Custody** — killed by **UC/Freestar/Neon security**. **Confirm:** "Security takes
      you into custody" then a vanilla jail transition (bounty/sentence/release all stock).
- [ ] **Left as a Message** — killed by **House Va'ruun** zealots. **Confirm:** no credit loss; a
      concussion affliction; "as a message" notification.
- [ ] **Picked Clean** — human aggressor (repeat until it wins the roll). **Confirm:** ~25% credits
      **and your digipicks** taken; a contusion.
- [ ] **Left for Scrap** — killed by a **robot/turret**. **Confirm:** "registers you as neutralized";
      an energy burn; short recovery. ***This is the open M0 test: positive `ActorTypeRobot` keyword.***
- [ ] **Mauled** — killed by a **creature**. **Confirm:** "loses interest and prowls off"; lacerations;
      long down; does not re-fire in the same cell. ***Positive `ActorTypeCreature` keyword test.***
- [ ] With `explainDecisions` on, **confirm** the log shows the filter reasons + priority/weight roll.
- [ ] **Wake-up destination** (new 2026-07-30, unverified): the five non-arrest consequences now exit via
      `OSFDefeatBasicRecovery.FinishAftermath()`. With no tagged marker placed, **confirm** recovery is
      unchanged (same spot, no fade) and the log shows `no same-cell recovery marker found; using local
      recovery` — this is the zero-regression check. Then place a test XMarker carrying
      `OSFDefeatBasicRecoveryMarker` (000807 in `OSFDefeatBasic.esm`) in the same cell and **confirm**
      the fade-out → move → recover → fade-in sequence runs.

## 3. M4 capture hooks (console harness)

Run `cgf "OSFCaptureTest.Help"` first. Get into a fight for a live target.

- [ ] `cgf "OSFCaptureTest.DownTarget"` — **confirm:** your combat target drops (log `registered
      defeated actor`), scripted cause `2`.
- [ ] `cgf "OSFCaptureTest.CaptureTarget"` — **confirm:** log `captured XXXXXXXX (captor 00000014)`;
      the NPC stays down and stops being shot at (captive faction applied).
- [ ] `cgf "OSFCaptureTest.ListCaptives"` — **confirm:** notification count ≥ 1; log lines show
      `captor=<player>` and `isCaptive=True`.
- [ ] `cgf "OSFCaptureTest.TransferToPlayer"` then `cgf "OSFCaptureTest.CheckDistance"` — **confirm:**
      distance drops to near-zero; log `transfer captive ... dispatch ok`. ***This is the one part with
      no proven precedent — the `ObjectReference.MoveTo` VM-dispatch. If the captive does not move,
      the dispatch path needs revisiting (arg packing / handle type).***
- [ ] Save mid-capture, reload, `ListCaptives` again — **confirm:** the roster survives (`OSFD`/`CAP0` in the `.osf` sidecar);
      log `loaded ... / saved N capture relationship record(s)`.
- [ ] `cgf "OSFCaptureTest.ReleaseAll"` — **confirm:** captives recover; `ListCaptives` now reports 0.

## 4. Trust / safety invariants

- [ ] Watchdog: hold a consequence past its `leaseTimeoutSeconds` (e.g. a scenario whose script you block).
      **Confirm:** the tick force-recovers and logs the defaulting lease-holder ID.
- [ ] `cgf "OSFDefeat.ForceRecoverAll"` while defeated **and** holding captives. **Confirm:** everyone
      recovers, captive factions are stripped, and `ListCaptives` reports 0 (safe uninstall path).
- [ ] After ForceRecoverAll, save then inspect — **confirm:** the `OSFD` client block is absent from the `.osf` sidecar.
- [ ] `cgf "OSFDefeat.DisableProcessing" ...`/config `processingEnabled:false` → take a lethal hit.
      **Confirm:** vanilla death proceeds (framework steps aside); passthrough logged with a reason.

## Notes

- Remove `OSFCaptureTest.pex` when done; it is a test tool, not shipped content.
- If a keyword-gated scenario (Scrap/Mauled) never fires, the aggressor may not carry the expected
  `ActorType*` keyword — capture its base form and re-check the token before assuming a bug.
