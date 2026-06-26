#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

// =============================================================================
// Defeat — the actor-agnostic verbs that put an actor into and out of the defeat
// state. These own the engine manipulation; the pipeline orchestrates them and
// the API exposes them. Player and NPC go through the same entry points.
//
// All verbs must be called on the game thread.
// =============================================================================
namespace OSFDefeat::Defeat
{
	// Notification hooks. The API layer installs these so it can broadcast events
	// without the core having a compile-time dependency on the API (inverted dep).
	using Observer = std::function<void(FormID a_victim, FormID a_aggressor)>;
	void SetOnDefeat(Observer a_observer);
	void SetOnRescue(Observer a_observer);

	// Put an actor down. Returns false if it can't be defeated (dead / already
	// defeated). aggressor may be null.
	bool DefeatActor(RE::Actor* a_victim, RE::Actor* a_aggressor);

	// Bring a defeated actor back up. Returns false if not defeated / busy.
	bool RescueActor(RE::Actor* a_victim, bool a_undoPacify);

	[[nodiscard]] bool IsDefeated(const RE::Actor* a_victim);
	[[nodiscard]] bool IsDamageImmune(const RE::Actor* a_victim);

	[[nodiscard]] std::vector<RE::Actor*> GetAllDefeated(bool a_loadedOnly);

	// Re-apply down state for an actor restored from a co-save (no observer fire). recoverAtDay is
	// the persisted absolute game-time deadline, carried straight into the registry.
	void RestoreFromSave(FormID a_id, float a_registeredDay, FormID a_aggressor, float a_recoverAtDay);

	// --- Pacify: API shape reserved; per-actor pacify on Starfield is still TODO.
	void               Pacify(RE::Actor* a_victim);
	void               UndoPacify(RE::Actor* a_victim);
	[[nodiscard]] bool IsPacified(const RE::Actor* a_victim);
}
