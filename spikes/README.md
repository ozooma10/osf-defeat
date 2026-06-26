# Spikes — two throwaway probes that gate the foundation re-architecture

These answer the two unknowns the "OSF Defeat → generic, Acheron-style foundation"
plan hangs on, **before** committing to the phase plan. Throwaway: delete
`src/Harness/Spike.*`, this `spikes/` folder, and the `[SPIKE]` line in `src/main.cpp`
once you have your answers.

| Spike | Question | Gates |
|---|---|---|
| **A** | Can C++ register a native Papyrus global, and marshal a state-mutating call onto the game thread? | The whole Papyrus seam (Phase 1) |
| **B** | Can the foundation hand a defeated actor to a **content** quest (fill its reference alias with the victim)? | The Resolution → actor handoff (Phase 2/3) |

## What the RE already told us (so Spike B is framed correctly)

Pure-C++ reference-alias force-fill is **not available** in the CommonLibSF copy in
`lib/`: `BGSRefAlias`/`BGSBaseAlias` are RTTI/VTABLE-only (no class body), `TESQuest`
exposes no alias accessor, and there is no `ForceRefTo` AddrLib id. So Acheron's exact
"C++ fills the alias" mechanism does **not** port as-is.

Spike B therefore tests the **viable** handoff: C++ receives the `(quest, actor)` refs,
then pushes them back into **content Papyrus** via `IVirtualMachine::DispatchStaticCall`,
and the content script (`OSFSpikeContent.Fill`) does the `ForceRefTo`. If that works, the
Acheron-style "select one quest, hand it the victim" model is viable on Starfield with a
thin Papyrus fill-shim on the content side.

## Build

The C++ side builds with the plugin — no extra steps:

```bat
xmake build
```

`Spike::Register()` is wired to SFSE `kPostDataLoad` in `main.cpp`. If the log shows
`VM not ready at registration`, move the `Spike::Register()` call to `kPostPostDataLoad`
(inside `InstallCore`) instead.

## Test plugin (one tiny .esp)

Compile the three scripts in this folder against your Starfield Papyrus setup and build a
throwaway `OSFSpike.esp` (CK or xEdit):

1. `OSFSpike.psc` and `OSFSpikeContent.psc` are `Hidden` global-native/global holders —
   just compile them so the names resolve.
2. Create quest **`OSFSpikeQuest`**: Start Game Enabled, and add **one Reference Alias**
   (id 0; "Optional" so C++/Papyrus can fill it). Attach **`OSFSpikeQuestScript`** to the
   quest. (If the single alias gets an id other than 0, change the `GetAlias(0)` arg in
   `OSFSpikeContent.psc`.)
3. Enable `OSFSpike.esp` alongside the OSF Defeat plugin.

## Run + read the logs

Logs: `Documents\My Games\Starfield\SFSE\Logs\OSFDefeat.log` (the C++ `[OSFSpike]` lines)
and `Documents\My Games\Starfield\Logs\Script\Papyrus.0.log` (the `Debug.Trace` lines).
Load a save (fires `OnQuestInit`), or run `cqf OSFSpikeQuest RunSpikes` in the console.

### Spike A — PASS criteria

```
[OSFSpike] native globals bound: OSFSpike.Ping, OSFSpike.RunHandoff      <- bind worked
[OSFSpike][A] Ping native called: tag='hello-from-papyrus' callerTID=...  <- native invoked from Papyrus
[OSFSpike][A] marshaled task ran: taskTID=... callerTID=... differ=... player=00000014   <- game-thread marshaling
```
and in Papyrus.0.log: `[OSFSpike][A] driver: Ping() returned True` (return value marshaled
back). All three ⇒ **the Papyrus seam is viable**; proceed with Phase 1.

If `Ping native called` never appears, native registration failed (check the bind log line
and the script/object name match `OSFSpike`). If the marshaled task line never appears,
`SFSE::GetTaskInterface()` is unavailable at that point.

### Spike B — PASS criteria

In OSFDefeat.log:
```
[OSFSpike][B] RunHandoff native: quest=........ target=00000014 (C++ received both refs from Papyrus)
[OSFSpike][B] DispatchStaticCall(OSFSpikeContent.Fill) dispatched=true
```
In Papyrus.0.log:
```
[OSFSpike][B] content Fill: alias0 -> [Actor ...000014...] (wanted [Actor ...000014...])
```
`dispatched=true` **and** the alias readback equals the player ⇒ **C++→content push handoff
works**; the Acheron-style Resolution model ports (foundation pushes, content fills).

Failure modes and what they mean:
- `dispatched=false` → `DispatchStaticCall` can't be driven this way in this build. **Fallback:**
  use the SFSE message-event path instead (Spike A already proves Papyrus can be invoked) —
  the foundation broadcasts `OnActorDefeated(victim, aggressor)`, content registers for it and
  fills its **own** alias. Less deterministic targeting, but fully supported.
- `dispatched=true` but the alias readback is `None`/wrong → the alias id isn't 0, the alias
  isn't fillable, or `ReferenceAlias.ForceRefTo` behaves differently than assumed — fix the
  content shim, not the C++.

## After the spikes

- **Both pass** → the Phase 1/2 plan stands: native Papyrus API + a `quest` consequence
  effect that, on resolution, `DispatchStaticCall`s a content dispatcher with the victim/aggressor.
- **A passes, B's dispatch fails** → keep Phase 1, but the handoff becomes event-driven
  (broadcast + content-side fill) rather than a deterministic C++ push.
- Then delete this folder and the `[SPIKE]` wiring.
