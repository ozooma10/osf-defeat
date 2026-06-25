#pragma once

#include "pch.h"

// =============================================================================
// Validation — the gate the pipeline asks before processing a defeat. Stubs for
// now (everything allowed unless globally disabled), but wired in from day one so
// content/quests can be protected later (location/race/faction/effect exclusions,
// condition expressions) without re-plumbing the pipeline.
// =============================================================================
namespace OSFDefeat::Validation
{
	// Master switch (e.g. a quest entering a fragile scene). Thread-safe.
	void               SetEnabled(bool a_enabled);
	[[nodiscard]] bool IsEnabled();

	// Is the framework allowed to process a defeat right now / here?
	[[nodiscard]] bool CanProcess();

	// May this specific victim be defeated by this aggressor? aggressor may be null.
	[[nodiscard]] bool ValidatePair(RE::Actor* a_victim, RE::Actor* a_aggressor);
}
