#include "pch.h"

#include "Core/Decision.h"

#include "Core/Consequence.h"
#include "Engine/EngineFns.h"

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
		// Order per OSF RE actor_faction_taxonomy (all SAFE field reads — no ID-bound calls):
		// Crew -> Security -> Civilian -> Hostile.
		if (IsPlayerCrew(a_victim)) {
			return EncounterClass::Crew;
		}
		if (a_victim->boolBits.all(RE::Actor::BOOL_BITS::kGuard)) {
			return EncounterClass::Security;  // guard / law-enforcement (boolBits bit30)
		}
		// Civilian = has a crime faction (jurisdiction) but isn't currently fighting anyone. The
		// crime faction is the JURISDICTION, shared by a city's guards and civilians — kGuard above
		// already split off the guards, so a non-guard with a crime faction and no combat target is
		// a bystander.
		if (Engine::HasCrimeFaction(a_victim) && !Engine::HasCombatTarget(a_victim)) {
			return EncounterClass::Civilian;
		}
		return EncounterClass::Hostile;
	}

	const char* ClassName(EncounterClass a_class)
	{
		switch (a_class) {
		case EncounterClass::Hostile:
			return "Hostile";
		case EncounterClass::Crew:
			return "Crew";
		case EncounterClass::Security:
			return "Security";
		case EncounterClass::Civilian:
			return "Civilian";
		default:
			return "Unknown";
		}
	}

	bool IsDefeatExcluded(const RE::Actor* a_victim, const RE::Actor* a_aggressor)
	{
		// The player's own crew is never put into the framework defeat/bleedout-hold state — let
		// vanilla companion-down handle them.
		return Classify(a_victim, a_aggressor) == EncounterClass::Crew;
	}

	Outcome SelectOutcome(
		EncounterClass a_class, const RE::Actor* a_victim, const RE::Actor* a_aggressor)
	{
		const Outcome frameworkDefault{ /*hold*/ false, /*recoverySeconds*/ -1.0f };
		if (!a_victim) {
			return frameworkDefault;
		}
		const Context ctx{
			a_victim->GetFormID(),
			a_aggressor ? a_aggressor->GetFormID() : 0u,
			static_cast<std::uint32_t>(a_class),
		};
		// Data-driven rules (consequences.json) drive the framework's automatic outcome — mainly for
		// defeats no Papyrus consumer overrides (a consumer holds + rescues an actor itself).
		if (const auto outcome = Consequence::Evaluate(ctx)) {
			return *outcome;
		}
		return frameworkDefault;
	}
}
