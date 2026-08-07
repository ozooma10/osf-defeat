# Lethal-handling design — native hooks & defeat recipes

How OSF Defeat intercepts would-be-lethal outcomes and holds an actor in a framework-owned
defeat state, adapted to Starfield (not a blind Skyrim/Acheron port). Every AddrLib ID below is
proven against **1.16.244** (`versionlib-1-16-244-0.bin`); the canonical RE store is OSF RE's
`gameplay.defeat_damage_hook` module (Responses 2026-06-24 … 2026-06-26).

> History: an earlier draft of this file described an NPC-only `HookTrigger` + `PollTrigger` +
> `Pipeline` split under `src/Trigger/`. That design was never the shipped code. This document
> now tracks the actual implementation.

## Seams in use

| Purpose | Seam | AddrLib ID | Notes |
|---|---|---|---|
| Lethal-write clamp | `ActorValueOwner::ModActorValue` slot-6 | **101587** (`0x1419774f0`) | entry detour, 5-byte steal `48 89 5C 24 08`, 9-byte verify gate; slot-7 wrapper (101588) tail-calls it |
| 3D-reload re-assert | `Actor::Set3D` (vtable slot 0xA8) | **101138** (`0x141948610`) | entry detour, 5-byte steal `48 89 5C 24 10` |
| NPC combat removal | `ProcessLists::StopCombatAndAlarmOnActor` | **103285** | per-actor disengage (others drop the victim) |
| Aggressor resolve | `BSPointerHandle::GetSmartPointer` | **35638** | resolves `currentCombatTarget` (Actor+0x298) |
| Down/recover lifeState | `Actor::SetLifeState` (slot 0x16F) | 100787 | `kBleedOut=8` / `kAlive=0` |

All actor mutation runs on the **game thread**; the detours do arithmetic + a deferred SFSE task
only (no process-list walks, no worker-thread object mutation).

## Player path — clamp, don't divert

The `ModActorValue` detour ([src/Core/PlayerDefeat.cpp](src/Core/PlayerDefeat.cpp)) is **player-scoped**
(`IsPlayerAVOwner`). On a would-be-terminal write to the **Health** gauge,
`kDamage` modifier, `delta < 0`, and `pre + delta <= 0`, it **clamps the delta** so the gauge lands at
a small floor — the engine's terminal path never reads `<= 0`. Not a negate, not a lifeState reroute;
the only lever is the clamped delta, and the actor is left genuinely alive at the floor (no death race).

A clamp always yields a defeat: it only fires when `Validation::CanProcess() && ValidatePair(...) &&
!IsDefeated(player)`, then queues `BeginPlayerDefeat` to the game thread.

Down state ([Engine::SetPlayerDownState](src/Engine/Engine.cpp)): base+instance **Essential** + heal to
max + `kNoBleedoutRecovery` + `kCanSpeakToEssentialDown` + `SetGhost(true)` +
`SetLifeState(kBleedOut)` + `StopCombat` + `StopCombatAndAlarmOnActor`. Player AI is deliberately left
enabled so camera/look remain usable; the presentation scene owns input lock. `RestorePlayerState`
restores the exact pre-defeat AI/flag/ghost/life-state/essential snapshot.

### Aggressor attribution
`a_ref` (5th arg) is the real damage dealer on the direct combat path. On the slot-7 / Papyrus
`DamageValue` path `a_ref` is null → fall back to the victim's **`currentCombatTarget`** (Actor+0x298),
resolved via `GetSmartPointer` on the game thread. **Not `myKiller`** (0x29C) — it is only written once a
kill commits, which the clamp prevents (disproven in-game 2026-06-25).

## NPC path

NPC defeat uses the same held state via the **live-validated per-actor recipe**, driven two ways:
**explicitly** through the verb (`OSFDefeat.DefeatActor(npc, aggressor)`), and via an
**opt-in auto-defeat** (`npcAutoDefeat`, default off). When enabled, the clamp resolves the victim from the
hooked AV owner (`ActorFromAVOwner`) and gates on `Decision::IsHostile` + flesh-policy (`IsNonFlesh`,
ID 42794 / 100890) — inline on the worker thread, symmetric with the player branch. Off by default because
auto-downing every hostile NPC is a large gameplay change that still wants an NPC consequence policy.

Worker-thread safety of those classification reads was **confirmed in the OSF RE sandbox (2026-06-27)**:
3,821 concurrent worker-thread `IsHostileToActor`/`HasKeyword` calls across 22 threads, zero faults, and a
game-thread cross-check matched (only `hostile` varied, a real combat-state change). So the inline classify
is safe; the `currentCombatTarget == player` proxy is unneeded. `ActorTypeRobot`/`ActorTypeTurret` resolve
by editorID; the true-on-robot positive is the one low-risk runtime item left.

Down state ([Engine::SetNpcDownState](src/Engine/Engine.cpp)): `kNoBleedoutRecovery` + `SetGhost(true)`
(invulnerable, so the defeat sticks) + `SetLifeState(kBleedOut)` + `StopCombat` +
`StopCombatAndAlarmOnActor` (others disengage). **No** essential flip and no global combat-AI gate.
`RestoreNpcState` heals and restores the exact pre-defeat AI/flag/ghost/life
state. `Pacify`/`UndoPacify` toggle per-actor AI (`SetAIEnabled`, the `tai`/kProcessMe primitive) —
never the player — and persist the prior AI value so release cannot blindly enable it.

`Defeat::ApplyDownState`/`RestoreState` dispatch player-vs-NPC so every call site (defeat, rescue, save
restore, 3D re-assert) routes to the right recipe and ownership snapshot.

## 3D-reload re-assert

`SetLifeState`/`SetGhost`/`kNoBleedoutRecovery` are silently stripped when an actor's 3D rebuilds (cell
change / save load / `coc`). The `Actor::Set3D` entry hook ([src/Core/Reattach.cpp](src/Core/Reattach.cpp))
detects an **attach** (`a_new3D != null`) of a still-defeated actor, lets the engine finish, then queues an
idempotent `ApplyDownState` re-assert for the next frame — replacing a lifeState poll.

## Recovery & persistence

A 500 ms worker queues `TickRecovery` on the game thread, which rescues any defeated actor past its
`recoverAtDay` deadline. Held entries (`recoverAtDay <= 0`, e.g. an NPC awaiting explicit content
recovery) wait for `RescueActor`. State persists through OSF Animation's `.osf` sidecar (`OSFD` client,
backward-compatible `DFT0` v2 plus capture/pacify ownership records) and is re-applied on the game
thread after load. Revert advances a runtime epoch, so deferred work from the prior save cannot land
in the newly loaded state.

## Dead ends (proven false — do not retry)

- **`Actor::HandleHealthDamage` (63465)** as the clamp point: post-write notifier; HP already `<= 0`.
  (It is still the universal *damage signal* — all 20 callers, incl. explosion/melee/projectile/script.)
- **Rewriting `Actor::SetLifeState` (100787)** kDying→kBleedOut: death commits through more than the
  lifeState (boolBits/teardown), so the instance flip doesn't take. Good *detector*, wrong *divert*.

## Files

- [src/Core/PlayerDefeat.cpp](src/Core/PlayerDefeat.cpp) — ModActorValue clamp, attribution, recovery worker.
- [src/Engine/Engine.cpp](src/Engine/Engine.cpp) — player/NPC down & restore recipes, AV + combat-target helpers.
- [src/Core/Defeat.cpp](src/Core/Defeat.cpp) — registry verbs + dispatch.
- [src/Core/Reattach.cpp](src/Core/Reattach.cpp) — Set3D re-assert hook.
- [src/Papyrus/Functions.cpp](src/Papyrus/Functions.cpp) — Papyrus API surface (defeat/rescue/pacify verbs, events, classification).

## In-game validation

Log: `Documents\My Games\Starfield\SFSE\Logs\OSFDefeat.log`.

1. **Install** — expect `installed player lethal damage hook at {X}` and `installed 3D-reattach re-assert
   hook at {X}`. A `prologue mismatch` line means the seam drifted on a game update; that hook
   self-disables cleanly and the rest is unaffected.
2. **Player Health** — take a lethal hit → defeat (bleedout, ghost, disengage), recover after the timer.
3. **Attribution** — a console `damageav health 9999` while fighting an NPC attributes that NPC (via
   `currentCombatTarget`); a non-combat `damageav` stays `00000000`.
5. **NPC defeat** — with `npcAutoDefeat` enabled, a would-be-lethal hit on a hostile NPC drops it into
   bleedout+ghost and others disengage; it recovers on the timer (or via a content `OSFDefeat.RescueActor`).
6. **3D re-assert** — defeat, then `coc` round-trip → state holds.

### Fallback
Each hook is independently prologue-gated and self-disables on drift. To hard-disable a path, drop its
`Install()` call in [src/main.cpp](src/main.cpp).
