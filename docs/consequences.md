# OSF Defeat — Resolution & consequences (Phase 2/3 target)

How the foundation turns "an actor went down" into "this authored content scene runs, with the
victim handed to it." This is **seam #2** — the actor-handoff that closes OSF Defeat's biggest gap
versus Acheron's Resolution. It grows the existing `Core/Consequence` + `data/consequences.json`,
it does not replace them.

Status: design target. The `quest` handoff mechanism is gated on **Spike B** (push vs broadcast).

## Today

`consequences.json` maps `class → {disposition: release|hold, recoverySeconds}` plus `event`/`log`
effects. It can set recovery timing and broadcast a tag — it **cannot** hand a defeated actor to a
content quest. That's the gap.

## Grown schema

Add Acheron's selection model (conditions / priority / weight / cooldown) and a `quest` effect:

```jsonc
{
  "version": 2,
  "rules": [
    {
      "class": "Hostile",
      "conditions": {
        "assailant": { "faction": ["0x5C84D|Starfield.esm"], "raceType": "Human" },
        "victim":    { },
        "quest":     { "running": [], "completed": [] }
      },
      "priority": 1,            // 0..3; highest non-empty tier is drawn from
      "weight": 50,            // weighted random within the chosen tier
      "cooldown": 30,          // seconds before this rule can fire again
      "requirements": ["NAFSurrender.esp"],
      "outcome": { "disposition": "hold" },   // foundation keeps the victim down until rescued
      "effects": [
        { "type": "quest", "form": "0x801|NAFSurrender.esp",
          "fill": { "victim": "Alias_Victim", "aggressor": "Alias_Captor" },
          "teleport": false },
        { "type": "event", "tag": "naf_capture" },
        { "type": "log", "message": "capture handoff" }
      ]
    }
  ]
}
```

### Selection (mirrors `Acheron::Resolution`)

1. Registered C++ outcome providers win first (unchanged).
2. Else evaluate rules: filter by `class` + `conditions` + `requirements`, group by `priority`,
   weighted-random within the highest non-empty tier, honoring `cooldown`.
3. Apply `outcome.disposition` (timing) and fire `effects` (what happens).

`disposition` stays strictly about **timing** (`release` / `hold` / `default`). "A content quest
now owns this victim" is expressed by the `quest` **effect**, not a new disposition — keep the two
orthogonal.

## The handoff effect

`{ "type": "quest" }` resolves the quest form, hands it the victim + aggressor, and starts it.
**The fill mechanism depends on the Spike B runtime result:**

- **Push (preferred, if `DispatchStaticCall` works):** C++ `DispatchStaticCall`s a content dispatcher
  — `OSFDefeatContent.OnHandoff(Quest, Actor victim, Actor aggressor)` — which fills its own alias
  (`ReferenceAlias.ForceRefTo`) and starts the quest. Deterministic: the *selected* quest runs.
  (Pure-C++ alias fill is unavailable in this CommonLibSF — the content Papyrus shim does the fill.)
- **Broadcast (fallback, if dispatch fails):** the foundation fires `OnActorDefeated`; the content
  quest is registered, decides for itself, fills its own alias, and starts. Less deterministic
  targeting, but uses only Phase-1 mechanisms.

Either way the **alias fill lives in content Papyrus**, never in foundation C++.

## Validation (seam #3, paired)

`validation.json` (parallel file) gates whether defeat may process here/now. Ship **exclusion lists
first** — they need no expression compiler, just form matching:

```jsonc
{
  "locationA": ["0x123|MyMod.esp"],         // suppress processing in these locations
  "locationT": ["0x456|MyMod.esp"],         // suppress teleport
  "exclusions": {
    "root":      [{ "race": "0x...|Starfield.esm" }],
    "assailant": [],
    "victim":    [{ "actorBase": "0x...|MyMod.esp" }]
  }
}
```

Defer the Acheron-style `GetStage 0xID > 250 AND` condition DSL until the Starfield predicate set
(`GetStage`/`GetQuestRunning`/`IsInScene`/…) is confirmed RE-able. Exclusion lists alone close most
of the "fragile scene got clamped" gap with far less risk.

## Code touch-points

- `Core/Consequence.{h,cpp}` — extend the rule struct (conditions/priority/weight/cooldown/
  requirements) and the effect enum (`+quest`); add the weighted-tier selector.
- `Core/Decision.cpp` — `SelectOutcome` already consults providers then `Consequence::Evaluate`;
  the `quest` effect fires in `Consequence::Fire` (game thread).
- `Core/Validation.cpp` — replace the crew-only stub with the `validation.json` exclusion loader.
