#pragma once

#include "pch.h"

// =============================================================================
// Decision — the policy layer between "an actor went down" and "what happens to
// it". Two jobs:
//   1. Classify the encounter (what KIND of actor/situation this is).
//   2. Select the Outcome (recovery policy) for a defeated actor, consulting the
//      data-rule layer (Consequence) and falling back to the framework default.
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
	// protected categories, exclusion in Validation. The value is also returned by the Papyrus
	// native OSFDefeat.ClassifyActor, so keep the numbering stable.
	enum class EncounterClass
	{
		Hostile = 0,   // an enemy combatant — the standard defeat
		Crew = 1,      // the player's own teammate / commanded actor — not a defeat target
		Security = 2,  // a guard / law-enforcement actor (boolBits kGuard)
		Civilian = 3,  // a non-combatant bystander (has a crime faction, not currently fighting)
		Unknown = 4,
	};

	[[nodiscard]] EncounterClass Classify(const RE::Actor* a_victim, const RE::Actor* a_aggressor);
	[[nodiscard]] const char*    ClassName(EncounterClass a_class);

	// True when this actor must NEVER enter the framework defeat state (e.g. the player's own
	// crew). Validation consults this so the HookTrigger clamp itself is skipped for them,
	// preserving the clamp<=>defeat invariant (no actor stranded clamped-but-undefeated).
	[[nodiscard]] bool IsDefeatExcluded(const RE::Actor* a_victim, const RE::Actor* a_aggressor);

	// What to do with an actor that IS being defeated (the recovery policy). recoverySeconds is
	// REAL seconds (Pipeline converts to game-time at apply); <0 means "use the default beat".
	struct Outcome
	{
		bool  hold = false;             // true -> held until an explicit rescue (recoverAtDay <= 0)
		float recoverySeconds = -1.0f;  // <0 -> default beat; >=0 -> explicit real-seconds span
	};

	// Identifies one defeat for the data-rule layer (Consequence). FormIDs + an EncounterClass value.
	struct Context
	{
		std::uint32_t victim;
		std::uint32_t aggressor;       // 0 if unknown
		std::uint32_t encounterClass;  // an EncounterClass value
	};

	// Select the recovery outcome for a defeat: the data-rule layer (Consequence) if a rule
	// matches, else the framework default (auto-recover after the beat).
	[[nodiscard]] Outcome SelectOutcome(
		EncounterClass a_class, const RE::Actor* a_victim, const RE::Actor* a_aggressor);
}
