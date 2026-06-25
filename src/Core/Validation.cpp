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

	bool CanProcess()
	{
		// TODO: location (LocationA) exclusions, scene checks.
		return g_enabled.load();
	}

	bool ValidatePair(RE::Actor* a_victim, RE::Actor* /*a_aggressor*/)
	{
		if (!a_victim) {
			return false;
		}
		// TODO: reference / actor-base / race / faction exclusions + condition exprs.
		return true;
	}
}
