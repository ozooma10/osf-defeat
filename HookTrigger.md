# HookTrigger — pre-death damage hook (Phase 2)

Second `ITrigger` implementation. Catches a would-be-lethal hit **before** the kill commits,
attributes the aggressor, and routes the victim into the existing held-bleedout defeat state.
This is the trigger `PollTrigger` could never be: an NPC takes a lethal hit and dies in one
frame with no bleedout window to poll, so the lethal hit itself must be intercepted.

## The RE'd seam (from OSF RE `gameplay.defeat_damage_hook`, PHASE 4, live 2026-06-25, 1.16.244)

- **`ActorValueOwner::ModActorValue` slot-6 impl = AddrLib ID `101587` = `0x1419774f0`**
  - `void(ActorValueOwner* /*rcx*/, ACTOR_VALUE_MODIFIER mod /*edx*/, const ActorValueInfo& info /*r8*/, float delta /*xmm3*/, TESObjectREFR* a_ref /*5th, stack*/)`
  - The slot-7 wrapper (ID `101588`, `a_ref = null`) **tail-calls** slot-6, so one entry hook
    on slot-6 catches both the wrapper (Papyrus/script) and the direct combat callers.
- **Entry detour:** steal 5 bytes `48 89 5C 24 08` (`mov [rsp+8],rbx`, clean boundary);
  verify-gate 9 bytes `48 89 5C 24 08 89 54 24 10`. Bound by **raw AddrLib ID** (this
  CommonLibSF copy is unsynced — same pattern as `EngineFns`).
- **Mechanism (Acheron-style):** damage is written here **before** the death decision reads the
  post value. Clamp the Health `delta` so post-hit Health lands at a small floor (`5.0`), so the
  engine's downstream kill path never fires. **Not a negate** — always forward to the gateway;
  the only lever is the clamped delta. The actor is left **alive at floor HP** → no death race.
- **Aggressor:** the `a_ref` arg is the actual damage dealer on the direct combat path
  (validated by the seam body via `cmp [a_ref+0x2e],0x4b`). On the wrapper/script path `a_ref`
  is null → attributed null for now (see Follow-ups).

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

1. **Install** — on load: `HookTrigger installed (ModActorValue pre-death clamp; NPC scope)`.
   If instead you see `ModActorValue ID 101587 gate failed`, the prologue drifted (game update)
   — the hook self-disables and NPCs simply have no trigger; the player is unaffected.
2. **NPC defeat** — get a hostile NPC into normal combat and land a killing blow. Expect a line:
   `hook: lethal hit on XXXXXXXX (preHp=… dmg=…) clamped to floor 5.0; aggressor YYYYYYYY; prevented death -> defeat`
   followed by the existing `NPC defeat (bleedout + ghost + disengage)` / `defeated …`.
   The NPC should **drop into bleedout, not die**, others disengage, and it survives further fire
   (ghost). Aggressor `YYYYYYYY` should be **you** (or the actual attacker).
3. **Attribution** — confirm `aggressor` matches the real damage dealer on weapon kills. On a
   Papyrus/script `DamageValue` kill it will read `00000000` (expected; see Follow-ups).
4. **No regression** — player still goes down via the essential/poll path (bleedout, `tcai`
   disengage, ~8 s revive) exactly as before.
5. **Recovery** — defeated NPC auto-revives after the pipeline beat (`kBeatMs`, 8 s) and gets up.

### Fallback
To run **poll-only** (player only) — e.g. if the hook drifts on a future game build — drop the
`InstallTrigger(... HookTrigger ...)` line in `InstallCore` ([src/main.cpp](src/main.cpp)). The
gate-failure path already self-disables the hook cleanly, so this is only for a hard opt-out.

## Follow-ups
- **Aggressor on the script path:** wire the `GetSmartPointer` primitive (OSF RE ID `35638`) into
  `Engine` to resolve the victim's `myKiller` handle (Actor+0x29C) when `a_ref` is null. Deferred
  to avoid a worker-thread deref through this unsynced CommonLibSF's (possibly stale) handle table.
- **EM damage:** confirm whether EM/stun weapons deplete the Health AV through this seam or a
  separate gauge (open gap in the module). If separate, a would-be-lethal EM hit won't be caught.
- **Player via the hook:** if a unified hook path is ever wanted, it needs a player DefeatActor
  variant that drives bleedout directly (not via Essential), since the clamp keeps HP > 0.
