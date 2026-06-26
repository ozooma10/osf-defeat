# HookTrigger — pre-death damage hook (Phase 2)

Second `ITrigger` implementation. Catches a would-be-terminal hit **before** the engine commits
its terminal state — a lethal **Health** hit (→ death) or a would-be-incapacitating **EM Health**
hit (→ vanilla EM shock) — attributes the aggressor, and routes the victim into the existing
held-bleedout defeat state. This is the trigger `PollTrigger` could never be: an NPC takes a lethal
hit and dies in one frame with no bleedout window to poll, so the hit itself must be intercepted.

## The RE'd seam (from OSF RE `gameplay.defeat_damage_hook`, PHASE 4, live 2026-06-25, 1.16.244)

- **`ActorValueOwner::ModActorValue` slot-6 impl = AddrLib ID `101587` = `0x1419774f0`**
  - `void(ActorValueOwner* /*rcx*/, ACTOR_VALUE_MODIFIER mod /*edx*/, const ActorValueInfo& info /*r8*/, float delta /*xmm3*/, TESObjectREFR* a_ref /*5th, stack*/)`
  - The slot-7 wrapper (ID `101588`, `a_ref = null`) **tail-calls** slot-6, so one entry hook
    on slot-6 catches both the wrapper (Papyrus/script) and the direct combat callers.
- **Entry detour:** steal 5 bytes `48 89 5C 24 08` (`mov [rsp+8],rbx`, clean boundary);
  verify-gate 9 bytes `48 89 5C 24 08 89 54 24 10`. Bound by **raw AddrLib ID** (this
  CommonLibSF copy is unsynced — same pattern as `EngineFns`).
- **Mechanism (Acheron-style):** damage is written here **before** the engine's terminal decision
  reads the post value. Clamp the `delta` so the post-hit gauge lands at a small floor (`kDownFloor
  = 5.0`), so the engine's downstream terminal path never fires. **Not a negate** — always forward
  to the gateway; the only lever is the clamped delta. The actor is left **alive at the floor** → no
  death race.
- **Two AVs reach a terminal state through this same primitive** (OSF RE `em-stun-damage-routing`,
  2026-06-25): **Health** → death, and **Electromagnetic Health** → the vanilla EM
  shock/incapacitation. EM/stun depletes a *separate* "Electromagnetic Health" gauge, not Health, so
  the thunk matches **both AVIFs** and clamps them identically (one floor, one code path). EM Health
  AVIF is resolved off `BGSDefaultObjectManager::GetSingleton()` (ID `43134`) `+ 0xd0` — see
  `Engine::EMHealthAV()`. **Decision = CLAMP, not ADOPT:** keep one framework-owned defeat/recovery
  state rather than layering ours on top of the engine's behavior-graph shock (which is also
  recoverable, but invisible to the framework and needs the unresolved shock-state RE to wire onto).
- **Aggressor:** the `a_ref` arg is the actual damage dealer on the direct combat path
  (validated by the seam body via `cmp [a_ref+0x2e],0x4b`). On the wrapper/script path `a_ref`
  is null → `Drain` falls back to the victim's `currentCombatTarget` (see Follow-ups).

### Dead ends (proven false in the sandbox — do not retry)
- Hooking the keystone `HandleHealthDamage` (ID 63465): **post-write**, HP already ≤ 0.
- Rewriting `SetLifeState` (ID 100787) kDying→kBleedOut: death commits through more than the
  lifeState; the essential→bleedout reroute is **base-flag-gated**, so instance flips don't take.

## Design

- **Files:** [src/Trigger/HookTrigger.h](src/Trigger/HookTrigger.h),
  [src/Trigger/HookTrigger.cpp](src/Trigger/HookTrigger.cpp),
  [src/Engine/Hook.h](src/Engine/Hook.h) (trampoline entry-hook helper, ported from the
  sandbox's proven `InstallEntryHookWithGateway`). Wired in [src/main.cpp](src/main.cpp).
- **Trampoline:** allocated via `SFSE::Init(a_sfse, { .trampoline = true, .trampolineSize = 256 })`.
- **Thread model:** the detour runs on the damage **worker thread** — it only does arithmetic +
  forwards the (clamped) call, and queues `{victim, aggressor}`. A per-frame SFSE task drains the
  queue on the **game thread** and fires `Pipeline::OnDown` → `Defeat::DefeatActor` (the proven
  recipe: `SetLifeState(8=kBleedOut)` + `kNoBleedoutRecovery` + `SetGhost(true)` +
  `StopCombatAndAlarmOnActor`). No process-list walks (respects the crash lesson).
- **Candidacy gate == pipeline gate.** The thunk clamps only when
  `Validation::CanProcess() && ValidatePair(victim, aggressor)` (both worker-safe today), so a
  clamp always yields a defeat — no actor stranded alive-but-undefeated at floor HP.
- **Scope = NPCs.** The player is excluded (`IsPlayerRef`) and stays on `PollTrigger`'s proven
  base-Essential → engine-bleedout path; clamping the player's HP to a floor would suppress that
  bleedout UX entirely. Both triggers compose on the same idempotent sink.

## In-game validation (what to check)

Log: `Documents\My Games\Starfield\SFSE\Logs\OSFDefeat.log`.

1. **Install** — on load: `HookTrigger installed (ModActorValue clamp: Health->death + EM
   Health->shock; NPC scope; EM AVIF resolved=true)`. `EM AVIF resolved=false` means the
   DefaultObjectManager getter/slot didn't resolve at install (it retries lazily on first use, so a
   later EM hit can still work — but false here is worth noting). If instead you see `ModActorValue
   ID 101587 gate failed`, the prologue drifted (game update) — the hook self-disables and NPCs
   simply have no trigger; the player is unaffected.
2. **NPC defeat (weapon/Health)** — get a hostile NPC into normal combat and land a killing blow.
   Expect a line:
   `hook: would-be-down Health hit on XXXXXXXX (pre=… dmg=…) clamped to floor 5.0; aggressor YYYYYYYY; prevented death -> defeat`
   followed by the existing `NPC defeat (bleedout + ghost + disengage)` / `defeated …`.
   The NPC should **drop into bleedout, not die**, others disengage, and it survives further fire
   (ghost). Aggressor `YYYYYYYY` should be **you** (or the actual attacker).
2b. **NPC defeat (EM/stun)** — deplete a hostile NPC's EM gauge with an EM weapon. Expect the same
   flow but `hook: would-be-down EM hit on XXXXXXXX … prevented EM shock -> defeat` — the NPC drops
   into our bleedout instead of the vanilla EM shock. **Not yet live-validated** (RE couldn't fire an
   EM weapon); this is the key thing to confirm. If an EM kill instead plays the vanilla shock with
   no `EM hit` line, check `EM AVIF resolved` at install (offset drift).
3. **Attribution** — confirm `aggressor` matches the real damage dealer on weapon kills
   (live-proven 2026-06-25: weapon kill of FF0157FE logged `aggressor 00000014` via `a_ref`). On a
   Papyrus/script `DamageValue` kill of an NPC **actively fighting you**, watch for
   `hook: script-path aggressor resolved via combatTarget -> YYYYYYYY` (the `currentCombatTarget`
   fallback). A non-combat NPC has no combat target → `aggressor 00000000` (expected).
4. **No regression** — player still goes down via the essential/poll path (bleedout, `tcai`
   disengage, ~8 s revive) exactly as before.
5. **Recovery** — defeated NPC auto-revives after the pipeline beat (`kBeatMs`, 8 s) and gets up.

### Fallback
To run **poll-only** (player only) — e.g. if the hook drifts on a future game build — drop the
`InstallTrigger(... HookTrigger ...)` line in `InstallCore` ([src/main.cpp](src/main.cpp)). The
gate-failure path already self-disables the hook cleanly, so this is only for a hard opt-out.

## Follow-ups
- **Aggressor on the script path — DONE + LIVE-VALIDATED 2026-06-25 (combatTarget).** Proof: a
  console `damageav health 9999` on an NPC fighting the player logged
  `script-path aggressor resolved via combatTarget -> 00000014` and `defeated FF017625 (aggressor
  00000014)` — clamp on the worker thread (a_ref null → 0), resolved on the game-thread `Drain` via
  `GetSmartPointer` (no crash).
  When `a_ref` is null (the slot-7 / Papyrus `DamageValue` path), `Drain` falls back to
  `Engine::ResolveCombatTarget`, which reads the victim's `currentCombatTarget` handle (Actor+0x298,
  "who they were fighting") and resolves it via `GetSmartPointer` (.244 ID `35638`, current in this
  copy's `IDs.h`). The resolve runs on the **game thread** (in `Drain`, not the worker thunk) under
  the handle manager's reader lock.
  - **Why not `myKiller`?** First implemented against `myKiller@0x29C`; **disproven in-game
    2026-06-25** — a console `damageav health 9999` clamp logged `aggressor 00000000` and NO
    `resolved via myKiller` line. The clamp *prevents* the kill, and `myKiller` is only written when
    a kill **commits** (the `killer=00000014` proof was on the `SetLifeState` death path we bypass),
    so it reads empty for clamped victims. `currentCombatTarget` is populated throughout the fight,
    so it's the correct signal.
  - **Still to validate:** `damageav`-kill an NPC that is **actively fighting you** → expect
    `hook: script-path aggressor resolved via combatTarget -> 00000014`. A non-combat NPC has no
    combat target → stays `00000000` (correct: nobody defeated it in combat).
- **EM damage — RESOLVED + IMPLEMENTED 2026-06-26 (CLAMP; awaiting in-game validation).** OSF RE
  `em-stun-damage-routing` confirmed EM/stun depletes a *separate* "Electromagnetic Health" AV, but
  through the **same** `ModActorValue` primitive (ID 101587) we already hook — so this was a wider AV
  filter, not a new seam. The thunk now also matches the EM Health AVIF (`Engine::EMHealthAV()`, via
  `BGSDefaultObjectManager` getter ID 43134 + slot 0xd0) and clamps it identically. Vanilla EM "kill"
  is a timed, recoverable shock (not death); we CLAMP so the shock never fires and the victim enters
  our unified `DefeatActor` state instead (see the Design "Two AVs" bullet for the CLAMP-vs-ADOPT
  rationale). **Pending runtime cross-checks** (RE couldn't fire an EM weapon): (1) confirm an EM
  down logs the `EM hit` line and drops the NPC into our bleedout; (2) keystone `HandleHealthDamage`
  (ID 63465) should show 0 hits under EM damage (EM ≠ Health). Floor `5.0` assumes a 0–100-ish EM
  gauge; revisit if the EM Health max turns out tiny.
- **Player via the hook:** if a unified hook path is ever wanted, it needs a player DefeatActor
  variant that drives bleedout directly (not via Essential), since the clamp keeps HP > 0.
