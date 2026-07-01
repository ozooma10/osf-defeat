#pragma once

#include "pch.h"

namespace OSFDefeat::Validation
{
	void SetEnabled(bool a_enabled);
	[[nodiscard]] bool IsEnabled();
	[[nodiscard]] bool CanProcess();
	[[nodiscard]] bool ValidatePair(RE::Actor* a_victim, RE::Actor* a_aggressor);

	// Why a lethal hit was (or would be) passed through instead of clamped. Every deliberate
	// passthrough is loggable with its reason — the "legible death boundary": users should be able
	// to answer "why didn't it save me" from the log.
	enum class Verdict
	{
		kOk,
		kApiDisabled,         // DisableProcessing(true) — a mod asked us to stand down
		kConfigDisabled,      // processingEnabled: false in validation JSON
		kNoVictim,
		kVictimDead,          // already dead when we looked (scripted kill committed first)
		kInScene,             // disableInScenes gate
		kLocationExcluded,    // excludedLocations gate
	};
	[[nodiscard]] Verdict ExplainPair(RE::Actor* a_victim, RE::Actor* a_aggressor);
	[[nodiscard]] std::string_view VerdictName(Verdict a_verdict);
}
