# OSF Defeat — One-Launch Verification Plan

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
