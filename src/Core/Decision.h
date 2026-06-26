#pragma once

#include "pch.h"

#include "API/OSFDefeatAPI.h"  // OutcomeProvider / DefeatContext / DefeatOutcome (public ABI types)

// =============================================================================
// Decision — the policy layer between "an actor went down" and "what happens to
// it". Two jobs:
//   1. Classify the encounter (what KIND of actor/situation this is).
//   2. Select the Outcome for a defeated actor (recovery policy today; pluggable /
//      data-driven consequences later).
// Plus the exclusion predicate Validation consults so protected actors (e.g. the
// player's own crew) are never clamped/defeated in the first place.
//
// Classification is BEST-EFFORT: it reads modeled actor flags (safe memory reads),
// so a misread mis-classifies but never crashes. It deliberately avoids ID-bound
// engine calls (e.g. IsHostileToActor) because this CommonLibSF copy is unsynced
// and those IDs aren't verified on 1.16.244.
// =============================================================================
namespace OSFDefeat::Decision
{
	// Coarse encounter category for a downed actor. Drives outcome selection and, for the
	// protected categories, exclusion in Validation.
	enum class EncounterClass
	{
		Hostile,  // an enemy combatant — the standard defeat
		Crew,     // the player's own teammate / commanded actor — not a defeat target
		Unknown,
	};

	[[nodiscard]] EncounterClass Classify(const RE::Actor* a_victim, const RE::Actor* a_aggressor);
	[[nodiscard]] const char*    ClassName(EncounterClass a_class);

	// True when this actor must NEVER enter the framework defeat state (e.g. the player's own
	// crew). Validation consults this so the HookTrigger clamp itself is skipped for them,
	// preserving the clamp<=>defeat invariant (no actor stranded clamped-but-undefeated).
	[[nodiscard]] bool IsDefeatExcluded(const RE::Actor* a_victim, const RE::Actor* a_aggressor);

	// What to do with an actor that IS being defeated. Today this is purely the recovery policy;
	// Phase 3 grows it (data-driven; hostility resolution; consequence hooks). recoverySeconds is
	// REAL seconds (Pipeline converts to game-time at apply); <0 means "use the default beat".
	struct Outcome
	{
		bool  hold = false;             // true -> held until an explicit rescue (recoverAtDay <= 0)
		float recoverySeconds = -1.0f;  // <0 -> default beat; >=0 -> explicit real-seconds span
	};

	// Selects the outcome for a defeat: consults registered outcome providers (first non-default
	// verdict wins), else returns the framework default (auto-recover after the beat).
	[[nodiscard]] Outcome SelectOutcome(
		EncounterClass a_class, const RE::Actor* a_victim, const RE::Actor* a_aggressor);

	// --- Outcome provider registry (the public API's RegisterOutcomeProvider forwards here, so the
	// registry lives in Core and SelectOutcome can consult it without a Core->API dependency).
	// Thread-safe; providers are consulted on the game thread, copied out before invocation so a
	// provider may freely (un)register afterwards.
	std::uint32_t RegisterOutcomeProvider(OSFDefeatAPI::OutcomeProvider a_provider, void* a_userData);
	bool          UnregisterOutcomeProvider(std::uint32_t a_token);
}
