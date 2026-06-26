#include "pch.h"

#include "Core/Validation.h"

#include "Core/Decision.h"

namespace OSFDefeat::Validation
{
	namespace
	{
		std::atomic<bool> g_enabled{ true };
	}

	void SetEnabled(bool a_enabled) { g_enabled.store(a_enabled); }
	bool IsEnabled() { return g_enabled.load(); }

	bool CanProcess()
	{
		// TODO: location (LocationA) exclusions, scene checks.
		return g_enabled.load();
	}

	bool ValidatePair(RE::Actor* a_victim, RE::Actor* a_aggressor)
	{
		if (!a_victim) {
			return false;
		}
		// Actor-taxonomy exclusions: the player's own crew is never clamped/defeated (let vanilla
		// companion-down handle them). Rejecting here (rather than in the pipeline outcome) keeps
		// the HookTrigger clamp<=>defeat invariant — the clamp is skipped too, so no excluded actor
		// is stranded clamped-but-undefeated. Runs on the damage worker thread: flag reads only.
		if (Decision::IsDefeatExcluded(a_victim, a_aggressor)) {
			return false;
		}
		// TODO: reference / actor-base / race exclusions + condition exprs (security/civilian split).
		return true;
	}
}
