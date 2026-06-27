#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Defeat
{
	using Observer = std::function<void(FormID a_victim, FormID a_aggressor)>;

	void SetOnDefeat(Observer a_observer);
	void SetOnRescue(Observer a_observer);

	bool DefeatActor(RE::Actor* a_victim, RE::Actor* a_aggressor);
	bool RescueActor(RE::Actor* a_victim, bool a_undoPacify);

	[[nodiscard]] bool IsDefeated(const RE::Actor* a_victim);
	[[nodiscard]] bool IsDamageImmune(const RE::Actor* a_victim);
	[[nodiscard]] std::vector<RE::Actor*> GetAllDefeated(bool a_loadedOnly);

	void RestoreFromSave(FormID a_id, float a_registeredDay, FormID a_aggressor);

	void               Pacify(RE::Actor* a_victim);
	void               UndoPacify(RE::Actor* a_victim);
	[[nodiscard]] bool IsPacified(const RE::Actor* a_victim);
}
