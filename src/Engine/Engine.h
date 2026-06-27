#pragma once

#include "pch.h"

namespace OSFDefeat::Engine
{
	constexpr RE::ACTOR_LIFE_STATE kAlive = RE::ACTOR_LIFE_STATE::kAlive;
	constexpr RE::ACTOR_LIFE_STATE kBleedOut = RE::ACTOR_LIFE_STATE::kBleedOut;

	[[nodiscard]] RE::ActorValueInfo* HealthAV();
	[[nodiscard]] RE::ActorValueInfo* EMHealthAV();
	[[nodiscard]] RE::ActorValueOwner* AVOwnerOf(RE::Actor* a_actor);
	[[nodiscard]] float GetHealth(RE::Actor* a_actor);
	[[nodiscard]] float GetMaxHealth(RE::Actor* a_actor);

	// Resolves who a_victim was fighting (currentCombatTarget @ Actor+0x298) to an Actor.
	// Must be called on the game thread (handle-manager reader lock).
	[[nodiscard]] RE::Actor* ResolveCombatTarget(RE::Actor* a_victim);

	void RestoreHealthToMax(RE::Actor* a_actor);
	void SetCombatAIDisabled(bool a_disabled);
	void SetPlayerEssential(bool a_essential);
	void SetPlayerDownState(RE::Actor* a_actor);
	void RestorePlayerState(RE::Actor* a_actor);

	// Per-actor NPC down/recover (no essential flip, no global combat-AI gate — those are the
	// player path's levers). Live-validated recipe from OSF RE npc-defeat-recover-pacify, 2026-06-25.
	void SetNpcDownState(RE::Actor* a_actor);
	void RestoreNpcState(RE::Actor* a_actor);

	// Dispatch by actor: the player gets the essential/global-suppression recipe, NPCs the per-actor one.
	void ApplyDownState(RE::Actor* a_actor);
	void RestoreState(RE::Actor* a_actor);
}
