#include "pch.h"

#include "Core/Validation.h"

namespace OSFDefeat::Validation
{
	namespace
	{
		std::atomic<bool> g_enabled{ true };
	}

	void SetEnabled(bool a_enabled) { g_enabled.store(a_enabled); }
	bool IsEnabled() { return g_enabled.load(); }
	bool CanProcess() { return IsEnabled(); }

	bool ValidatePair(RE::Actor* a_victim, RE::Actor* /*a_aggressor*/)
	{
		return a_victim && !a_victim->IsDead();
	}
}
