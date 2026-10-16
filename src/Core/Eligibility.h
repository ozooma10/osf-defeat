#pragma once

#include "pch.h"

namespace OSFDefeat::Eligibility
{
	void SetEnabled(bool a_enabled);
	[[nodiscard]] bool IsEnabled();
	[[nodiscard]] bool IsEligible(RE::Actor* a_victim);

	// Why a lethal hit was (or would be) passed through instead of clamped.
	enum class Verdict
	{
		kOk,
		kApiDisabled,
		kConfigDisabled,
		kNoVictim,
		kVictimDead,
		kInScene,
	};
	[[nodiscard]] Verdict Explain(RE::Actor* a_victim);
	[[nodiscard]] std::string_view VerdictName(Verdict a_verdict);
}
