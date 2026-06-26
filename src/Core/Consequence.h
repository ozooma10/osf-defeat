#pragma once

#include "pch.h"

#include "API/OSFDefeatAPI.h"  // DefeatContext / DefeatOutcome (public ABI types)

#include <optional>

// =============================================================================
// Consequence — the data-driven outcome ruleset. A JSON file (deployed next to the
// DLL as OSFDefeat/consequences.json) maps encounter conditions to outcomes, so a
// content author can author defeat dispositions (release timing / hold) WITHOUT C++
// (the platform has no Papyrus seam; data files + the C++ ABI are the content seam).
//
// It speaks the PUBLIC ABI types (DefeatContext/DefeatOutcome), NOT Decision's, so the
// dependency stays one-directional (Decision -> Consequence). Decision consults it
// after the registered C++ providers and before the built-in default.
//
// This is the first slice of the "consequence engine": it drives the OUTCOME today;
// richer authored effects (item transfer, faction shifts, custom events) layer on later.
// =============================================================================
namespace OSFDefeat::Consequence
{
	// Load (or reload) the ruleset from the data file. Game thread, called once at data load.
	// Never throws: a missing/invalid file logs and leaves the ruleset empty (built-in defaults).
	void Load();

	// First matching rule's outcome for this defeat, or nullopt if no rule matches.
	[[nodiscard]] std::optional<OSFDefeatAPI::DefeatOutcome> Evaluate(
		const OSFDefeatAPI::DefeatContext& a_context);

	// Sink the framework calls to broadcast an authored "event" consequence. Installed by the API
	// layer (so Core has no API dependency — same inversion as Defeat's defeat/rescue observers).
	using EventSink = std::function<void(std::uint32_t a_victim, std::uint32_t a_aggressor, const char* a_tag)>;
	void SetEventSink(EventSink a_sink);

	// Execute the authored consequences (effects) of the rule matching this defeat: broadcast
	// "event" effects via the sink and write "log" effects. Game thread. Independent of who set the
	// outcome (a C++ provider overriding the recovery does NOT suppress a matched rule's effects).
	void Fire(const OSFDefeatAPI::DefeatContext& a_context);
}
