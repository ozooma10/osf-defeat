#pragma once

// =============================================================================
// Papyrus — the public Papyrus API binding layer (seam #1). Exposes the
// foundation's verbs/queries to Starfield Papyrus so a .esp/Papyrus content mod
// (NAF Surrender) consumes the framework natively instead of reimplementing
// detection. Thin skin over the existing core: Core/Defeat, Core/DefeatRegistry,
// Core/Decision, Core/Validation, Core/Pipeline.
//
// Binding mechanism proven by src/Harness/Spike.cpp (Spike A): globals tag the
// self slot with std::monostate; state-mutating verbs marshal onto the game
// thread via SFSE::GetTaskInterface()->AddTask.
//
// Phase 1 scope: verbs + queries only. The OnActorDefeated event (Papyrus
// SendEvent) and first-class Pacify (Phase 6) land later.
// =============================================================================
namespace OSFDefeat::Papyrus
{
	// Bind the native global functions under the "OSFDefeat" script object. Idempotent.
	// Call once when the VM is up (wired to SFSE kPostDataLoad in main.cpp).
	void Register();
}
