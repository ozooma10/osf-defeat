#pragma once

#include "pch.h"

namespace OSFDefeat::Engine
{
	constexpr RE::ACTOR_LIFE_STATE kAlive = RE::ACTOR_LIFE_STATE::kAlive;
	constexpr RE::ACTOR_LIFE_STATE kBleedOut = RE::ACTOR_LIFE_STATE::kBleedOut;

	[[nodiscard]] RE::ActorValueInfo* HealthAV();
	[[nodiscard]] RE::ActorValueOwner* AVOwnerOf(RE::Actor* a_actor);
	[[nodiscard]] float GetHealth(RE::Actor* a_actor);
	[[nodiscard]] float GetMaxHealth(RE::Actor* a_actor);

	// Resolves who a_victim was fighting (currentCombatTarget @ Actor+0x298) to an Actor.
	// Must be called on the game thread (handle-manager reader lock).
	[[nodiscard]] RE::Actor* ResolveCombatTarget(RE::Actor* a_victim);

	void RestoreHealthToMax(RE::Actor* a_actor);
	void SetPlayerEssential(bool a_essential);
	void SetPlayerDownState(RE::Actor* a_actor);
	void RestorePlayerState(RE::Actor* a_actor);

	// Per-actor NPC down/recover (no essential flip, no faction pacify — those are the player path's
	// levers). Live-validated recipe from OSF RE npc-defeat-recover-pacify, 2026-06-25.
	void SetNpcDownState(RE::Actor* a_actor);
	void RestoreNpcState(RE::Actor* a_actor);

	// Dispatch by actor: the player gets the essential + target-drop recipe, NPCs the per-actor one.
	void ApplyDownState(RE::Actor* a_actor);
	void RestoreState(RE::Actor* a_actor);

	// Captive-faction pacify, exposed on demand for content ("surrender to base"): adds a_actor to the configured captiveFaction so hostiles treat them as non-hostile.
	bool ApplyCaptiveFaction(RE::Actor* a_actor);
	bool RemoveCaptiveFaction(RE::Actor* a_actor);
	[[nodiscard]] bool HasCaptiveFactionApplied(RE::Actor* a_actor);

	// Co-save support for the captive tracking (so a save/load mid-captivity still removes exactly the membership we introduced).
	[[nodiscard]] std::vector<std::pair<std::uint32_t, std::uint32_t>> SnapshotCaptiveFactions();  // (actorID, factionID)
	void RestoreCaptiveFaction(std::uint32_t a_actorId, std::uint32_t a_factionId);
	void ClearCaptiveFactions();
}
