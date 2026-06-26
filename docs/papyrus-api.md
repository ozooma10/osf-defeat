# OSF Defeat — Papyrus API (Phase 1 target)

The native Papyrus surface a content mod (NAF Surrender) compiles against. This is **seam #1**
and the prerequisite for every later phase. The binding mechanism is proven by **Spike A**
(`src/Harness/Spike.cpp`): `RE::GameVM::GetSingleton()->GetVM()->BindNativeMethod(...)`, global
functions tag the self slot with `std::monostate`, state-mutating calls marshal onto the game
thread via `SFSE::GetTaskInterface()->AddTask`.

Status: design target. Implement as `src/Papyrus/Functions.cpp` once Spike A is **runtime**-
validated. The C++ verbs forward to the existing core: `Core/Defeat`, `Core/DefeatRegistry`,
`Core/Decision`.

## The script

```papyrus
Scriptname OSFDefeat Hidden
{ Public API for the OSF Defeat foundation. Drop this .psc in your content mod's source. }

; ---------------------------------------------------------------------------
; State verbs. Marshaled onto the game thread; the returned bool means
; "request accepted/queued", not "completed" (mirrors the C++ IVDefeat1 ABI).
; ---------------------------------------------------------------------------
bool Function DefeatActor(Actor akVictim, Actor akAggressor = None) global native
bool Function RescueActor(Actor akVictim, bool abUndoPacify = true) global native

; Pacify = hold an actor combat-neutral WITHOUT defeating it (Phase 6 / first-class).
; afSeconds < 0 => hold until ReleaseActor. Replaces SAF's calm-spell.
bool Function PacifyActor(Actor akActor, float afSeconds = -1.0) global native
Function ReleaseActor(Actor akActor) global native

; ---------------------------------------------------------------------------
; Queries. Safe from any thread (read-only over the registry).
; ---------------------------------------------------------------------------
bool Function IsDefeated(Actor akActor) global native
bool Function IsPacified(Actor akActor) global native
Actor[] Function GetDefeated(bool abLoadedOnly = false) global native

; "Hostile" | "Crew" | "Security" | "Civilian" | "Unknown" (Decision::Classify).
; akAggressor lets Classify resolve the Security/guard case.
String Function ClassifyActor(Actor akVictim, Actor akAggressor = None) global native

; ---------------------------------------------------------------------------
; Guard seam. Suspend the foundation around a content-authored scripted scene
; (capture cell, AuctionHouse teleport) so a stray hit can't clamp mid-scene.
; ---------------------------------------------------------------------------
Function DisableProcessing(bool abDisable) global native   ; wraps Validation::SetEnabled
Function DisableConsequence(bool abDisable) global native  ; suppress resolution handoff only

; ---------------------------------------------------------------------------
; Events. Register a Form (quest/alias/active script) to receive callbacks.
; The event delivers the aggressor + class so content branches WITHOUT
; re-reading engine memory (human/creature = GetRace on akAggressor; guard
; = IsGuard/GetCrimeFaction on akAggressor — all base-game Papyrus).
; ---------------------------------------------------------------------------
Function RegisterForDefeat(Form akReceiver) global native
Function UnregisterForDefeat(Form akReceiver) global native
; Event OnActorDefeated(Actor akVictim, Actor akAggressor, String asEncounterClass)
; Event OnActorRescued(Actor akVictim, Actor akAggressor)
```

## C++ wiring (sketch)

`Spike::Register()` already proves the bind. The real module differs only in scope:

```cpp
// src/Papyrus/Functions.cpp  (Phase 1)
bool DefeatActor(std::monostate, RE::Actor* a_victim, RE::Actor* a_aggressor)
{
    return API::Server::GetSingleton()->DefeatActor(a_victim);  // already marshals to game thread
}
// ... RescueActor, IsDefeated, GetDefeated, ClassifyActor forward likewise.

void Register()
{
    const auto vm = RE::GameVM::GetSingleton() ? RE::GameVM::GetSingleton()->GetVM() : nullptr;
    if (!vm) return;
    vm->BindNativeMethod("OSFDefeat", "DefeatActor", &DefeatActor, std::nullopt, false);
    // ... bind the rest. Call from main.cpp OnMessage at kPostDataLoad.
}
```

The defeat/rescue **events** reuse the existing broadcast: `Defeat::SetOnDefeat` already fires on
every defeat (see `API/APIServer.cpp`). Phase 1 adds a Papyrus registration set and, in that
observer, also dispatches the Papyrus `OnActorDefeated` (with `Decision::Classify` widened to read
the aggressor and pass `EncounterClass`).

## Prerequisite fix

`Decision::Classify` currently ignores `a_aggressor` (`Decision.cpp:68`). Phase 1 wires it in so
`ClassifyActor` and the `asEncounterClass` event arg are correct for the Security/guard split.

## Notes

- No `RE::StaticFunctionTag` (that's Skyrim) — globals use `std::monostate`.
- `RRef`/`ActiveMagicEffect` receiver flavors from Acheron are dropped; `Form`-based registration
  only (no ActiveMagicEffect analog on Starfield).
- Verbs are intentionally a thin Papyrus skin over the C++ `IVDefeat1/2` ABI — a C++ consumer and
  a Papyrus consumer drive the exact same core.
