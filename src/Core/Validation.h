#pragma once

#include "pch.h"

namespace OSFDefeat::Validation
{
	void SetEnabled(bool a_enabled);
	[[nodiscard]] bool IsEnabled();
	[[nodiscard]] bool CanProcess();
	[[nodiscard]] bool ValidatePair(RE::Actor* a_victim, RE::Actor* a_aggressor);
}
