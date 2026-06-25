#pragma once

#include "pch.h"

// =============================================================================
// Engine — the low-level, version-specific RE'd primitives. Everything that pokes
// raw engine memory or relies on a reverse-engineered address lives here, isolated
// from the rest of the core so a game-build update only touches this file.
//
// All of these were validated in the OSF RE sandbox on Starfield 1.16.244.
// =============================================================================
namespace OSFDefeat::Engine
{
	// Flip the global combat-AI gate the `tcai` console command toggles. true ->
	// every NPC breaks off combat; false -> normal. (Player-scope suppression; the
	// per-actor equivalent for NPC defeat is still a TODO — see Defeat.cpp.)
	void SetCombatAIDisabled(bool a_disabled);

	// Cached "Health" ActorValueInfo, or nullptr if forms aren't loaded yet.
	[[nodiscard]] RE::ActorValueInfo* HealthAV();

	// The ActorValueOwner subobject (CommonLibSF doesn't model it as an Actor base).
	[[nodiscard]] RE::ActorValueOwner* AVOwnerOf(RE::Actor* a_actor);

	[[nodiscard]] float GetHealth(RE::Actor* a_actor);

	// Heal to permanent max -> the engine exits bleedout and plays get-up.
	void RestoreHealthToMax(RE::Actor* a_actor);

	// Push current Health down to ~0 so an essential/protected actor drops into
	// bleedout. Used by the (best-effort) NPC defeat path.
	void DriveHealthFloor(RE::Actor* a_actor);

	// Make the player base (TESNPC) Essential so a lethal hit routes to bleedout.
	// Idempotent; cheap to call every frame.
	void EnsureEssential(RE::PlayerCharacter* a_player);
}
