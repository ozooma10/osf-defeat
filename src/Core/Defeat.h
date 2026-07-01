#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Defeat
{
	using Observer = std::function<void(FormID a_victim, FormID a_aggressor)>;

	void SetOnDefeat(Observer a_observer);
	void SetOnRescue(Observer a_observer);

	bool DefeatActor(RE::Actor* a_victim, RE::Actor* a_aggressor, DefeatCause a_cause = DefeatCause::kScripted);
	bool RescueActor(RE::Actor* a_victim, bool a_undoPacify);

	// How a defeated actor went down; kNone when not defeated.
	[[nodiscard]] DefeatCause GetDefeatCause(const RE::Actor* a_victim);

	// Emergency escape / safe uninstall: rescues every defeated actor (overriding any lease),
	// releases every pacified actor, and strips every captive-faction membership we created, so
	// the co-save empties on the next save and the DLL can be removed mid-playthrough. Also the
	// user-facing unstick verb (console: cgf "OSFDefeat.ForceRecoverAll"). Returns the number of
	// actors rescued. Game thread.
	std::size_t ForceRecoverAll();

	[[nodiscard]] bool IsDefeated(const RE::Actor* a_victim);
	[[nodiscard]] bool IsDamageImmune(const RE::Actor* a_victim);
	[[nodiscard]] std::vector<RE::Actor*> GetAllDefeated(bool a_loadedOnly);

	void RestoreFromSave(FormID a_id, float a_registeredDay, FormID a_aggressor, float a_recoverAtDay,
		float a_leaseDeadlineDay = 0.0f, std::string a_leaseOwner = {}, DefeatCause a_cause = DefeatCause::kNone);

	void               Pacify(RE::Actor* a_victim);
	void               UndoPacify(RE::Actor* a_victim);
	[[nodiscard]] bool IsPacified(const RE::Actor* a_victim);
}
