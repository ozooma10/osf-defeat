#pragma once

#include "pch.h"

namespace OSFDefeat::Eligibility
{
	void SetEnabled(bool a_enabled);
	[[nodiscard]] bool IsEnabled();
	[[nodiscard]] bool CanProcess();
	[[nodiscard]] bool IsEligible(RE::Actor* a_victim, RE::Actor* a_aggressor);

	// Why a lethal hit was (or would be) passed through instead of clamped.
	enum class Verdict
	{
		kOk,
		kApiDisabled,
		kConfigDisabled,
		kNoVictim,
		kVictimDead,
		kInScene,
		kLocationExcluded,
	};
	[[nodiscard]] Verdict Explain(RE::Actor* a_victim, RE::Actor* a_aggressor);
	[[nodiscard]] std::string_view VerdictName(Verdict a_verdict);
}
