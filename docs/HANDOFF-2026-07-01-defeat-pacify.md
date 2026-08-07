# OSF Defeat — Session Handoff (2026-07-01): down-camera + combat-resume + faction-pacify

> Historical snapshot. It intentionally preserves the terminology and repository state used on
> 2026-07-01. Current code distinguishes AI suspension, captive-faction non-hostility, recovery,
> and defeat leases; see [VOCABULARY.md](VOCABULARY.md). Do not copy API names from this handoff.

Cold-start handoff for continuing on another machine. Covers two fixed bugs, the
architecture decision behind them, how the reference mods pacify, and an in-progress
OSF RE lead for the "proper" pacify. Read the workspace `AGENTS.md` first for routing.

## TL;DR — current state

- **Bug 1 (can't mouse-look/orbit while down): FIXED & user-confirmed in-game.**
- **Bug 2 (combat AI dead after recovery until `tcai`): RESOLVED by removing the pacify** —
  the shipped build no longer pauses combat during the down beat, so combat resumes cleanly.
  **This final no-pacify build is deployed but NOT yet re-confirmed in-game** (user pivoted to
  research before testing it). First thing to do on the new machine: launch + verify down→recover
  with no `tcai` and no freeze.
- **Optional enhancement (bring the "enemies back off while down" beat back, properly):**
  faction-based pacify (SAF Surrender's approach). Native `AddToFaction`/`RemoveFromFaction` are
  not in commonlibsf — an OSF RE pass to prove them is **in progress** (candidate cluster found,
  IDs recorded; live-proof pending). See §6–7.

## 1. The two bugs

Original report: (1) when the player goes down they can't mouse-move/look/orbit;
(2) when they get back up, combat AI stays disabled until the `tcai` console command.

## 2. Bug 1 — down camera (FIXED)

Root cause: the player-down path puts the player in `kBleedOut` (`Engine::SetPlayerDownState`),
which kills the engine's native third-person look. The down scene had fallen back to the scene-def
default camera `thirdperson_hold` (engine-native look → dead) after commit `e5ebbf1 "no scene orbit"`
removed the `scene_orbit` override. `scene_orbit` (OSF Animation) self-drives a FreeFly camera from
raw mouse input, so it works regardless of lifeState — it's the only look camera that functions while
downed.

Fix: restored `std::strncpy(opts.camera, "scene_orbit", …)` in
[src/Core/Presentation.cpp](../src/Core/Presentation.cpp) `ShowPlayerDown`. **Committed** as
`b412cbe "scene orbit again"`. User confirmed mouse-orbit works in-game.

## 3. Bug 2 — combat resume (RESOLVED by dropping the pacify)

The down beat used the global `tcai` combat-AI gate (`RE::Console::SetCombatAIProcessing`,
handler ID 66826) to pacify enemies. The problem is on the RESUME side and it is **fundamental**:

- Verified via a gate read-back log that the gate byte flips correctly (down=disabled, recover=enabled).
- Removing `StopCombatAndAlarmOnActor(player)` so enemies keep the player targeted → on recovery they
  **freeze**, stuck aiming at the down spot; `gate=0` alone does not re-tick their combat controllers.
- Disassembled the `tcai` handler (`0x140c65c40`): after flipping the gate it makes a **conditional
  call to a subsystem setting-change broadcast** (`handler+0x3a → 0x14365d420`, walks a callback table)
  that is what un-sticks combat. That call is gated on a **thread-context flag** (`[TLS[0]+0xBC]==0`)
  true only when the console runs the command — **not reproducible from an SFSE task**. Invoking the
  handler from our task flips the gate but the broadcast does not fire → enemies still frozen.
- `ProcessLists::AreHostileActorsNear` and `ToggleAI` are **unbound (ID 0)** in this commonlibsf →
  calling them hard-crashes ("Invalid ID: 0"). (An attempt to enumerate hostiles that way crashed;
  reverted.)

**Decision (user chose "drop the pacify, robust"):** removed the tcai gate from the player path
entirely. Enemies are never paused, so there's nothing to un-freeze; the player is `SetGhost(true)`
(invulnerable) while down, so enemies harmlessly keep firing and combat simply continues on recovery.

### Code changes in [src/Engine/Engine.cpp](../src/Engine/Engine.cpp) (working tree, drop-pacify):
- `SetPlayerDownState`: **removed** `SetCombatAIDisabled(true)` and `StopCombatAndAlarmOnActor(player)`.
  Kept `StopCombat()` (player's own). Comment block explains why (search "No combat-AI pacify").
- `RestorePlayerState`: **removed** the gate-restore call.
- **Removed** the intermediate dead-ends `ResumeCombatAI()` and `RewakeAggressor()` (don't re-add).
- `SetCombatAIDisabled(bool)` still exists + a gate read-back INFO log; now **only** used by
  `Persistence::Revert` as a defensive "combat AI on" reset on load (also cleans legacy saves).
- `src/Core/Defeat.cpp` and `src/Engine/Engine.h` are net-unchanged (a helper was added then removed).

## 4. Build / deploy / git state

- Build+deploy: `xmake` in `C:\Modding\Starfield\OSF Defeat` → auto-installs to
  `MO2\mods\OSF Defeat`. **Game must be closed** or the DLL copy fails (file lock).
- Deployed DLL is the drop-pacify build (matches working tree).
- Git: `Presentation.cpp` fix committed (`b412cbe`). `Engine.cpp` drop-pacify is in the working tree;
  `git status` is slightly muddled from mid-session commits — **run `git status`/`git log`/`git diff`
  on the new machine and commit anything pending.** `docs/osf-surrender-port.md` is untracked
  (pre-existing, not from this session).
- Logs: `<Documents>\My Games\Starfield\SFSE\Logs\OSF Defeat.log` (+ `OSF Animation.log`). Resolve
  Documents via `[Environment]::GetFolderPath('MyDocuments')` (OneDrive redirect).

## 5. How the reference mods pacify (both AVOID the global gate)

- **SAF Surrender** (`C:\Modding\Starfield\SAF_Surrender_1_2_EspVersion`,
  `_SurrenderCalmSpellEffectScript.psc`): **faction-based**. On surrender:
  `player.StopCombatAlarm()` + `player.AddToFaction(CaptiveFaction)`; on end: `RemoveFromFaction`.
  `CaptiveFaction` = **vanilla `0x0003E0C8:Starfield.esm`** (NOT custom). Enemies go non-hostile via
  faction reactions; recovery re-hostiles them through normal AI — no freeze. The author explicitly
  abandoned tcai ("this idea fucking do not work" / `ExecuteConsole("tcai")` unreliable).
- **Acheron** (`C:\Modding\Skyrim\Acheron`, `src/Acheron/Defeat.cpp`): **no global gate**. Per-actor
  `AddPackageOverride(BlankPackage)` + `EvaluatePackage` to make a defeated NPC inert; player gets
  `ControlMap::ToggleControls` (control lock); `PacifyUnsafe` = `ClearCachedFactionFightReactions` +
  `StopCombatAndAlarmOnActor`. Bleedout is an ANIMATION (`PlayIdle "BleedoutStart"`), not a lifestate
  divert. **Not directly portable to Starfield:** `EvaluatePackage`'s ID is unbound in your CLSF
  (crashes), `ClearCachedFactionFightReactions` isn't in Starfield CLSF, needs PapyrusUtil + a Package.
- Conceptual lesson (Acheron): "defeated" usually means the fight is OVER → transition to a consequence,
  not "resume the same fight." Matches [docs/osf-surrender-port.md](osf-surrender-port.md) — OSF Surrender
  consumes the down-state; sustained pacify is really content's job.

## 6. Chosen path for pacify: native faction calls (RE in progress)

Decision fork was: native VM-dispatch vs an OSFDefeat.esm+Papyrus vs leave-to-content. User asked to
**RE a native `AddToFaction`** so OSF Defeat stays pure-native and ESM-free. Plan once proven:
config-gated `captiveFaction` FormID (default `0x0003E0C8`), `AddToFaction` on player-down,
`RemoveFromFaction` on recover. `Spriggit` (`C:\Modding\Starfield\Tools\Spriggit\Spriggit.CLI.exe`,
`-p Spriggit.Yaml -v 0.40 -g Starfield -d <gameData> -u false`) works and was used to decompile SAF's
ESP — but **no custom ESM is needed** since 0x3E0C8 is vanilla.

## 7. OSF RE pass — Actor::AddToFaction / RemoveFromFaction (OPEN)

Full record: `C:\Modding\Starfield\OSF RE\Investigations\Requests\2026-07-01-actor-addtofaction-removefromfaction.md`
(untracked — commit it). Tools: `tools/re/exe_query.py`, `tools/re/addrlib_query.py`
(**always `--version 1-16-244-0`** — default is stale .242), `tools/ghidra/ghidra_headless.py` (bridge
configured), sandbox `tools/dev/Start-StarfieldDebug.ps1` → `MO2\mods\starfield-re-sandbox`.

Proven so far (static, 1.16.244):
- `ExtraFactionChanges`: extra type **0x56**, vtable ID **399102** (`0x144b25310`), RTTI 845505,
  **ctor ID 45534** (`0x140549480`, object 0x40 bytes, entry list ~+0x20).
- **Faction-method cluster = AddrLib IDs 45015–45021** (`0x140528890`–`0x1405294b0`, consecutive =
  the 7 faction methods). `0x140528dc0` (ID 45018) inline-creates ExtraFactionChanges and takes a
  `char rank` arg (add/set candidate). ExtraFactionChanges helpers by the ctor: `FUN_140549540`,
  `FUN_1405495f0`.
- Papyrus `Actor.psc` has `RemoveFromFaction`/`SetFactionRank`/`ModFactionRank`/`GetFactionRank`/
  `IsInFaction`/`RemoveFromAllFactions` but NOT `AddToFaction` (registered native, no shipped decl;
  string `AddToFaction` @ `0x144bc0390`).

Remaining: decompile the 7 → map each to its Papyrus name (RemoveFromAllFactions=1 arg; getters
return int/bool; add/set/mod take rank); confirm entry-point shape (`this=Actor*` vs an ExtraDataList
worker); prologue-verify; **live-test in the DefeatHookProbe sandbox** (add player to 0x3E0C8 → enemies
calm; remove → re-engage) — not confirmed until runtime agrees. Then port to OSF Defeat + fill
commonlibsf `Actor::AddToFaction`/`RemoveFromFaction` (Skyrim CLSF mirror: `AddToFaction`
RELOCATION_ID(36678,37686), `RemoveFromFaction` 36680).

## 8. Open items / next steps
1. **Verify the shipped no-pacify build in-game** (down→recover, no tcai, no freeze, no crash).
2. Commit pending working-tree changes (Engine.cpp) + the untracked OSF RE investigation file.
3. If pacify wanted back: finish the AddToFaction RE (§7), then the small OSF Defeat port.
4. Not-yet-saved memory: the tcai-gate dead-end + SAF/Acheron pacify comparison + the AddToFaction
   RE lead. Worth writing to auto-memory so a future session doesn't re-derive it.
