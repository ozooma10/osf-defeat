#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Engine
{
	constexpr RE::ACTOR_LIFE_STATE kBleedOut = RE::ACTOR_LIFE_STATE::kBleedOut;

	[[nodiscard]] RE::ActorValueInfo* HealthAV();
	[[nodiscard]] RE::ActorValueOwner& AVOwnerOf(RE::Actor& a_actor);

	// Resolves who a_victim was fighting (currentCombatTarget @ Actor+0x298) to an Actor.
	// Must be called on the game thread (handle-manager reader lock).
	[[nodiscard]] RE::Actor* ResolveCombatTarget(RE::Actor* a_victim);

	// Core callers validate actor/record and hold RuntimeMutation::Mutex before entry.
	// Dispatch by actor: the player gets the essential + target-drop recipe, NPCs the per-actor one.
	void ApplyDownState(RE::Actor& a_actor, ActiveDefeat& a_defeat);
	void RestoreState(RE::Actor& a_actor, const ActiveDefeat& a_defeat, bool a_preserveFaction = false);
	void StopCombat(RE::Actor& a_actor);

	// Captive-faction non-hostility, exposed on demand for content ("surrender to base"): adds a_actor to the configured captiveFaction so hostiles treat them as non-hostile.
	bool ApplyCaptiveFaction(RE::Actor& a_actor);
	bool RemoveCaptiveFaction(RE::Actor& a_actor);
	[[nodiscard]] bool HasCaptiveFactionApplied(RE::Actor& a_actor);
	[[nodiscard]] bool IsInCaptiveFaction(RE::Actor& a_actor);
	[[nodiscard]] bool IsCaptiveFactionAvailable();

	// Co-save support for the captive tracking (so a save/load mid-captivity still removes exactly the membership we introduced).
	[[nodiscard]] std::vector<std::pair<std::uint32_t, std::uint32_t>> SnapshotCaptiveFactions();  // (actorID, factionID)
	void RestoreCaptiveFaction(std::uint32_t a_actorId, std::uint32_t a_factionId);
	void ResetRuntimeState();
}
