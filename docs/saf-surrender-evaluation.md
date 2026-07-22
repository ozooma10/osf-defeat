# SAF Surrender — Evaluation & Mapping onto OSF Defeat

Catalog of SAF Surrender to understand exactly what it does and how it'd map onto
OSF Defeat as a content mod.

## What SAF Surrender actually is

Despite the folder name it's **NAF Surrender v1.2** — a LoversLab-style adult combat-defeat mod, pure Papyrus + ESP, hard-dependent on **NAF (Starfield Animation Framework)**. It's monolithic: one `_SurrenderTriggerQuest` (~1534 lines) does *everything* — detection, pacify, scene state machine, and outcomes.

Its actual feature loop:

1. **Trigger** — `OnHit` on the player; if `Health < BaseHealth × 0.30`, surrender fires. No hotkey, no menu. Guards against self-hits, dead aggressors, and re-entry.
2. **Pacify** — brute-force console `tcai` + `tgm` for the scene; a "calm spell" + CaptiveFaction for capture. The author's own comments admit `StopCombat`, the calm spell, and captive faction `0x0003E0C8` *all failed to pacify*, so he fell back to `tcai`.
3. **Enemy gate** — only `HumanRace` enemies accept surrender; non-humans route to the creature end (off by default).
4. **Scene** — strip player (2 methods), 3-stage NAF animation state machine with moans/orgasm voice lines (306 audio files) and GIFT haptics log-tags.
5. **Outcome branching** — Ransom (pay credits) / Resist → capture-roll (`RandomInt(1,99)+levelDiff < 20`) / bad-ends (hogtie, breathplay, probe) / arrest (guards → `SendPlayerToJail`).
6. **Robbery** — moves items matching `SurrenderRobItemsFormList` into a chest, timer routes them to the attacker/boss.
7. **Capture** — `MoveTo` a random enemy-base marker, strip items into a locked boss chest, calm-spell keeps you "captive," dialogue scenes with duration modifiers, escape by leaving the location or equipping a weapon.
8. **Config** — no MCM; a `coc aaSurrenderCell` debug room with terminals editing ~45 globals.
9. **Vibrator device**, companion freezing, fade cinematics, AuctionHouse.esp teleport hook.

## The key architectural insight

**SAF Surrender is really two mods fused together** — a *framework* (detect defeat, pacify, hold the down-state) and *content* (scenes, robbery, capture, outcomes). It does the framework half *badly* — the whole `tcai`/`tgm` crutch exists because the author couldn't solve pacification cleanly, and the README warns it leaves AI/god-mode stuck if a scene is interrupted.

**That framework half is exactly what OSF Defeat already does correctly.** Your memory files even record the same discovery the author hit — tcai can't pacify a player-down, faction pacify is the right route — except you actually *solved* it (guarded `AddToFaction`/`RemoveFromFaction`, precise FormID tracking, co-save-safe). So rebuilding SAF Surrender as an OSF Defeat content mod isn't just parity — it *fixes SAF's central defect for free.*

## The split: who owns what

| SAF Surrender does it via… | In an OSF-Defeat world, owned by… | Status |
|---|---|---|
| `OnHit` threshold detection | **OSF Defeat** (`ModActorValue` clamp) | ✅ You already do this, more robustly (native hook on Health) |
| `tcai`/`tgm` pacify | **OSF Defeat** (faction pacify + target-list drop) | ✅ You already solved this |
| Down-state / god-mode / bleedout hold | **OSF Defeat** (`SetPlayerDownState`) | ✅ Done |
| Recovery timer / restore | **OSF Defeat** (recovery worker) | ✅ Done |
| "which consequence fires" selection | **OSF Defeat** (consequence JSON, weight/priority) | ✅ Scaffolded |
| Handoff of aggressor + outcome to content | **OSF Defeat** (`DefeatContext` + `GetLastPlayerAggressor` etc.) | ✅ Exists |
| **NAF animation scene** | **Content mod** | ⚠️ You pose via OSF Animation; content mod drives its own scenes |
| **Stripping / robbery** | **Content mod** | ❌ You force `stripMode=0` — content mod's job |
| **Capture / teleport / boss chest / dialogue** | **Content mod** | ❌ Not present |
| **Ransom / arrest / bad-ends** | **Content mod** | ❌ Not present |
| **Config (globals/terminals)** | **Content mod** (its own MCM/terminal) | ❌ Content mod's own |

## What's missing in OSF Defeat's *API* to host such a content mod

Your framework is ~80% ready to host this. The gaps are in the **handoff surface** — the content mod needs enough hooks to take over once you've downed the player:

1. **`quest` consequence field must actually start the quest.** This is the single biggest gap. SAF's entire flow is "defeat → start a quest that runs the scene." You parse/expose `quest` but never `Start()` it. Without this, a content mod has nothing to hang its scene on. *(You already flagged this as stubbed.)*
2. **A "consumer took over / hold recovery" handshake.** SAF holds the player down for a whole multi-stage scene + capture. You need the content mod to be able to say "I own this down-state now, don't auto-recover on the timer" and later "I'm done, rescue now." Your `recoverAtDay <= 0` held-entry state + `RescueActor` verb is *most* of this — it needs to be reachable/documented as the intended handoff, plus a "extend/suspend recovery" call.
3. **Pacify scope on demand.** SAF needs *base-wide* calm during capture (not just target-list drop). Your `factionPacify` does exactly this but is player-only, config-gated, and tied to defeat. A content mod wants to *request* faction-pacify explicitly (surrender to a whole base) and release it. Expose `factionPacify`/release as a Papyrus verb, not just an internal config toggle.
4. **Aggressor/participant list.** SAF builds a member list (attacker, boss, all nearby hostiles) for its scene. You expose the single last aggressor; a scene mod wants "who else is nearby and was hostile." Consider a `GetDefeatParticipants`-style query.
5. **Voluntary surrender input.** SAF triggers on health-threshold only, but a proper framework should let a content mod *initiate* defeat (surrender key). You have `DefeatActor(victim, aggressor)` as a Papyrus verb already — so a content mod can bind its own hotkey and call it. ✅ This one's actually covered; just document it.

Everything else SAF does — NAF scenes, robbery, capture teleport, ransom, bad-ends, vibrator, config — is legitimately *content-mod* territory and should **not** go in the framework.

## How it relates to Acheron

- **Acheron = framework + a reference content layer** (Hunter Pride menu, default jail/teleport consequences). It's the mature version of what you're building.
- **SAF Surrender = a monolith** that reinvented a *worse* framework because Starfield had no Acheron. Its consequence content (capture, robbery, ransom, arrest) maps almost 1:1 onto **Acheron's consequence-event system** (`Hostile`/`Guard`/`Civilian` event pools, teleport gating, `SendPlayerToJail`). SAF's "arrest guards → jail" is literally Acheron's `AcheronDefaultGuard`; SAF's capture-to-base is Acheron's teleport consequence.
- **Your target, stated cleanly:** OSF Defeat should be the *framework* (Acheron's core), and a "SAF Surrender-parity" content mod should be the *first consumer* — proving the handoff API the way Acheron's built-in consequences prove Acheron's.

## Recommendation

The right first milestone is **not** "port SAF Surrender." It's **"make OSF Defeat's handoff API rich enough that a SAF-parity content mod is possible,"** then build a thin proof-of-concept consumer. Concretely, the framework work is items 1–4 above:

1. Actually start the consequence `quest`.
2. Recovery suspend/hold + explicit rescue handshake.
3. `factionPacify`/release as Papyrus verbs (surrender-to-base).
4. Participant/aggressor query.

That's a small, well-scoped set of framework changes — everything else lives in the content mod.
