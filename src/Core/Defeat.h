#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Defeat
{
	using Observer = std::function<void(FormID a_victim, FormID a_aggressor)>;

	void SetOnDefeat(Observer a_observer);
	void SetOnRecovery(Observer a_observer);

	bool DefeatActor(RE::Actor* a_victim, RE::Actor* a_aggressor, DefeatCause a_cause = DefeatCause::kScripted);
	bool RecoverActor(RE::Actor* a_actor, bool a_resumeAI);

	// How a defeated actor went down; kNone when not defeated.
	[[nodiscard]] DefeatCause GetDefeatCause(const RE::Actor* a_victim);

	// Emergency escape / safe uninstall: recovers every defeated actor (overriding any lease),
	// resumes every framework-suspended actor's AI, and strips every owned captive-faction membership, so
	// the co-save empties on the next save and the DLL can be removed mid-playthrough. Also the
	// user-facing unstick verb (console: cgf "OSFDefeat.ForceRecoverAll"). Returns the number of
	// actors recovered. Game thread.
	std::size_t ForceRecoverAll();

	[[nodiscard]] bool IsDefeated(const RE::Actor* a_victim);
	[[nodiscard]] bool IsDamageImmune(const RE::Actor* a_victim);
	[[nodiscard]] std::vector<RE::Actor*> GetAllDefeated(bool a_loadedOnly);

	void RestoreFromSave(FormID a_id, float a_registeredDay, FormID a_aggressor, float a_autoRecoverAtDay,
		float a_leaseExpiresAtDay = 0.0f, std::string a_leaseHolderId = {}, DefeatCause a_cause = DefeatCause::kNone,
		ActorStateSnapshot a_priorState = {});

	void               SuspendAI(RE::Actor* a_actor);
	void               ResumeAI(RE::Actor* a_actor);
	[[nodiscard]] bool IsAISuspended(const RE::Actor* a_actor);

	// Persistence support for explicitly suspended actors. The bool is the AI-enabled state that
	// must be restored, so ResumeAI never blindly enables AI that another system had disabled.
	[[nodiscard]] std::vector<std::pair<FormID, bool>> SnapshotAISuspensions();
	void RestoreAISuspension(FormID a_id, bool a_wasAIEnabled);
	void ResetRuntimeState();
}
