#pragma once

#include "pch.h"

#include "Core/Decision.h"  // Decision::Context / Decision::Outcome

#include <optional>

// =============================================================================
// Consequence — the data-driven outcome ruleset. A JSON file (deployed next to the
// DLL as OSFDefeat/consequences.json) maps an encounter class to a recovery
// disposition (+ optional authored log effects).
//
// This is an INTERNAL layer now — the public content seam is the Papyrus API
// (OSFDefeat.psc). It drives the framework's DEFAULT outcome for defeats that no
// Papyrus consumer overrides (e.g. NPC defeats: a consumer holds + rescues an actor
// itself). Dependency is one-directional: Decision -> Consequence.
// =============================================================================
namespace OSFDefeat::Consequence
{
	// Load (or reload) the ruleset from the data file. Game thread, called once at data load.
	// Never throws: a missing/invalid file logs and leaves the ruleset empty (built-in defaults).
	void Load();

	// First matching rule's outcome for this defeat, or nullopt if no rule matches / declines.
	[[nodiscard]] std::optional<Decision::Outcome> Evaluate(const Decision::Context& a_context);

	// Execute the authored "log" effects of the matching rule (author-side testing). Game thread.
	void Fire(const Decision::Context& a_context);
}
