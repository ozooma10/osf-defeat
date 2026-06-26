#include "pch.h"

#include "Core/Decision.h"

namespace OSFDefeat::Decision
{
	namespace
	{
		// The player's own crew / companion / commanded actor. Modeled boolBits/boolFlags (the same
		// flag stores DefeatActor already drives), so these are safe reads on any thread.
		[[nodiscard]] bool IsPlayerCrew(const RE::Actor* a_actor)
		{
			return a_actor &&
			       (a_actor->boolBits.all(RE::Actor::BOOL_BITS::kPlayerTeammate) ||
				   a_actor->boolFlags.all(RE::Actor::BOOL_FLAGS::kIsCommandedActor));
		}
	}

	EncounterClass Classify(const RE::Actor* a_victim, const RE::Actor* /*a_aggressor*/)
	{
		if (!a_victim) {
			return EncounterClass::Unknown;
		}
		if (IsPlayerCrew(a_victim)) {
			return EncounterClass::Crew;
		}
		// TODO (taxonomy): a security/civilian split needs faction / crime-faction data, which is
		// ID-bound on this unsynced CLSF (unverified on .244) — deferred. Everything else, including
		// the normal enemy combatant, defaults to Hostile.
		return EncounterClass::Hostile;
	}

	const char* ClassName(EncounterClass a_class)
	{
		switch (a_class) {
		case EncounterClass::Hostile:
			return "Hostile";
		case EncounterClass::Crew:
			return "Crew";
		default:
			return "Unknown";
		}
	}

	bool IsDefeatExcluded(const RE::Actor* a_victim, const RE::Actor* a_aggressor)
	{
		// The player's own crew is never put into the framework defeat/bleedout-hold state — let
		// vanilla companion-down handle them. (Mirrors the Phase-4 companion exclusion.)
		return Classify(a_victim, a_aggressor) == EncounterClass::Crew;
	}

	Outcome SelectOutcome(
		EncounterClass a_class, const RE::Actor* /*a_victim*/, const RE::Actor* /*a_aggressor*/)
	{
		// Scaffold policy: every currently-defeatable class auto-recovers after the default beat.
		// The seam exists so Phase 3 can diverge (capture = hold; security = a longer beat; etc.)
		// and so mods can register outcome providers through the API.
		(void)a_class;
		return Outcome{ /*hold*/ false, /*recoverySpanDays*/ -1.0f };
	}
}
