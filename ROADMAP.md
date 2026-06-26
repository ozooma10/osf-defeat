# OSF Defeat — Roadmap: foundation + content split

Turning OSF Defeat from a self-contained framework into a **generic, Acheron-style defeat
foundation** that a separate **content** mod (NAF Surrender) builds on. This is the plan of
record; the per-seam detail lives in [`docs/`](docs/).

## Decisions (locked)

- **Foundation name stays `OSF Defeat`.** No rename. The namespace (`OSFDefeat`), public ABI
  header (`OSFDefeatAPI.h`), SFSE message ids (`'OSFr'`/`'OSFd'`/…), co-save id, and the
  `OSFDefeat/` config folder are unchanged. "Defeat" is the genre-standard, mechanism-legible
  term; keeping the name makes the old "Phase 0 — rename before publishing the ABI" a no-op.
- **Content ships separately as `NAF Surrender`** — a Papyrus/`.esp` mod that consumes the
  foundation through its seams. It keeps its established name (user equity + discoverability +
  signals the NAF dependency) and stays clearly distinct from the SFW-neutral foundation.
- **Boundary principle:** the foundation owns *when/whether* an actor goes down and the generic,
  engine-coupled, save-persisted machinery of *being* down. The content mod owns *what the scene
  is*. If a non-adult defeat mod would reuse a behavior unchanged → foundation. If it encodes a
  specific authored experience → content.

## Foundation vs content

**Foundation (OSF Defeat) owns**
- Detection (`PollTrigger` player + `HookTrigger` NPC ModActorValue clamp + EM-Health).
- The down / pacify / rescue / recover state machine over `DefeatRegistry` + co-save persistence.
- Combat disengage and invulnerability as engine primitives (`SetCombatAIDisabled`, `SetGhost`,
  `StopCombatAndAlarmOnActor`) — replacing SAF's `tgm`/`tcai` console hacks.
- Encounter classification (`Decision::Classify`) + the aggressor ref delivered in the event.
- Validation (which actors/locations may be processed).
- The Resolution + actor-handoff engine (select a rule, hand the victim to a content scene).
- The on-defeat interaction-menu **registry** (content supplies the options).
- A **minimal** set of genuinely engine-coupled primitives (below) + the down-state marker.
- The Papyrus API + the SFSE/Papyrus event bus.

**Content (NAF Surrender) owns**
- The threshold value + human/creature + capture-% / bad-end-% routing policy.
- All NAF choreography (9×3 positions, the `.glb` taxonomy), voice/orgasm cadence, voice packs.
- The outcome quests (capture, robbery, bad-end, arrest), props, companion posing.
- Terminal config, AuctionHouse + GIFT integrations, the wearable device.
- The registered menu options (ransom/resist/rob/arrest) and their handlers.
- The `consequences.json` / `validation.json` entries pointing the foundation at its quests.

## Corrected over-reaches (what the foundation does NOT do)

Verified against source during design — these stay out of the foundation:

- **No NAF wrapper.** `PlayOnActor` / `LockActorForAnimation` / `SyncGraphs` are already
  Papyrus-native via `Import SAFScript`. The foundation stays animation-agnostic; the shared
  scene engine lives in the **content** mod (one `SurrenderScene.psc`, killing SAF's copy-paste).
  The foundation's only animation job is the **down-state marker** (keyword/AV) that NAF scene
  selection can gate on — the Acheron `AcheronDefeated`-as-OAR-gate analog.
- **No race re-derivation.** `Decision::Classify` currently ignores the aggressor; SAF's
  human/creature branch is a one-line base-game `GetRace()`. The foundation just **delivers the
  aggressor ref + EncounterClass in the event**; content calls `GetRace()`/`IsGuard()`/
  `GetCrimeFaction()` itself.
- **No base-game Papyrus absorbed.** `MoveTo` (teleport), `PlaceAtMe` (prop spawn),
  `RemoveItem(Credits)` (currency), `RemoveItem(formlist, container)` (rob) are clean Papyrus
  and stay in content. The foundation's primitive set is only what Papyrus genuinely can't do
  safely: the **collision toggle** (`tcl` replacement), a co-save-tracked **strip token**, a
  generic **"despawn this ref on rescue"** hook, and first-class **Pacify**.

## The seams

| # | Seam | Detail |
|---|---|---|
| 1 | Papyrus native API | [`docs/papyrus-api.md`](docs/papyrus-api.md) |
| 2 | Resolution + actor handoff | [`docs/consequences.md`](docs/consequences.md) |
| 3 | Validation (exclusion lists first, condition DSL later) | grows `Validation.cpp` |
| 4 | On-defeat interaction-menu registry | `AddOption` / `OnMenuSelect`; Message/terminal UI |
| 5 | Minimal engine primitives + down-state marker | collision, strip token, despawn-on-rescue |

Plus two behavioral seams Acheron has and we want: a **guard seam** (`DisableProcessing` /
`DisableConsequence`, so content can suspend the foundation around its own scripted scenes) and
a **"combat effectively over" gate** (only run a full consequence when ~one hostile remains, so
the handoff never fires mid-firefight).

## The spikes (the gate)

Before committing to Phases 1–3, two throwaway probes settle the two unknowns the whole plan
rests on. See [`spikes/README.md`](spikes/README.md).

| Spike | Question | Status |
|---|---|---|
| **A** | Native Papyrus global + game-thread marshaling | **compiles + links** ✓ · runtime run pending |
| **B** | C++ → content handoff (`DispatchStaticCall` push; alias fill in content Papyrus) | **compiles + links** ✓ · runtime run pending |

RE finding baked into Spike B: pure-C++ reference-alias force-fill is **not** exposed by this
CommonLibSF (`BGSRefAlias` is RTTI-only). So the handoff is either **push** (C++ `DispatchStaticCall`s
a content dispatcher, which fills its own alias) or, if dispatch fails at runtime, **broadcast**
(foundation fires `OnActorDefeated`; content fills its own alias). Both are designed for.

## Migration phases (each independently shippable)

1. **Papyrus binding layer** *(after Spike A)* — native `DefeatActor`/`RescueActor`/`IsDefeated`/
   `GetDefeated`/`ClassifyActor` + `RegisterForDefeat`/`OnActorDefeated`, marshaled to the game
   thread. *Ships:* any Papyrus mod can defeat/query an actor and receive the event.
2. **NAF Surrender consumes detection** — deletes its `OnHit` poll + `tgm`/`tcai`, keeps its own
   scene engine. *Ships:* SAF runs on the foundation with far less hackery — a real user win.
3. **Resolution handoff** *(after Spike B)* + classification payload + down-state marker.
   *Ships:* a content quest auto-runs on defeat with the victim handed in.
4. **Engine primitives** (collision toggle, strip token, despawn-on-rescue) + the shared content
   scene engine. *Ships:* zero `tcl`/`coc`; the capture quest reuses one scene engine.
5. **Validation exclusions + interaction-menu registry.** *Ships:* fragile cells excluded by data;
   the end menu becomes registered options.
6. **First-class Pacify** + NAF Surrender ships hack-free and separate. *Ships:* foundation
   installable alone by a non-adult defeat mod.

## Open risks

- **SFSE/CommonLibSF Papyrus-binding runtime maturity** (Spike A) — the largest unknown; gates Phase 1.
- **`DispatchStaticCall` push vs event broadcast** (Spike B) — determines whether Acheron's
  deterministic Resolution model ports or the handoff is event-driven.
- **NAF animation notify** — if Starfield/NAF exposes no reliable stage notify, content keeps
  hand-timed `Utility.Wait` cadences for voice/haptics; don't design a seam that depends on a tick.
- **RE offset fragility** — every new RE'd offset rides on 1.16.244 and re-anchors on game updates;
  keep the foundation's RE surface as small as the corrected boundary allows.
- **Condition-DSL predicate availability** — ship Validation as exclusion lists first; defer the
  AND/OR expression compiler until the Starfield predicate set is confirmed RE-able.
