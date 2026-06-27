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

	void RestoreHealthToMax(RE::Actor* a_actor);
	void SetCombatAIDisabled(bool a_disabled);
	void SetPlayerEssential(bool a_essential);
	void SetPlayerDownState(RE::Actor* a_actor);
	void RestorePlayerState(RE::Actor* a_actor);
}
