#include "pch.h"

#include "Core/Decision.h"

#include "Core/Consequence.h"

#include <algorithm>
#include <optional>

namespace OSFDefeat::Decision
{
	namespace
	{
		// Map a public verdict to an internal Outcome, or nullopt for kDefault ("decline" — defer
		// to the next source). Shared by the provider path and the data-rule path.
		[[nodiscard]] std::optional<Outcome> VerdictToOutcome(const OSFDefeatAPI::DefeatOutcome& a_verdict)
		{
			using Disposition = OSFDefeatAPI::DefeatOutcome::Disposition;
			switch (a_verdict.disposition) {
			case Disposition::kHold:
				return Outcome{ /*hold*/ true, /*recoverySeconds*/ -1.0f };
			case Disposition::kRelease:
				return Outcome{ /*hold*/ false, a_verdict.recoverySeconds };
			case Disposition::kDefault:
			default:
				return std::nullopt;
			}
		}

		// Internal EncounterClass and the public ABI enum must agree value-for-value (we pass the
		// internal value straight through to providers as DefeatContext::encounterClass).
		static_assert(static_cast<std::uint32_t>(EncounterClass::Hostile) ==
					  static_cast<std::uint32_t>(OSFDefeatAPI::EncounterClass::kHostile));
		static_assert(static_cast<std::uint32_t>(EncounterClass::Crew) ==
					  static_cast<std::uint32_t>(OSFDefeatAPI::EncounterClass::kCrew));
		static_assert(static_cast<std::uint32_t>(EncounterClass::Unknown) ==
					  static_cast<std::uint32_t>(OSFDefeatAPI::EncounterClass::kUnknown));

		// Registered outcome providers (from consumer plugins via the public API). Guarded because
		// (un)registration may arrive on a consumer thread while SelectOutcome consults on the game
		// thread; consultation copies the list out before invoking, so providers never run under the
		// lock (no re-entrancy deadlock, and a provider may (un)register afterwards).
		struct RegisteredProvider
		{
			std::uint32_t              token;
			OSFDefeatAPI::OutcomeProvider fn;
			void*                      userData;
		};
		std::mutex                       g_providersMutex;
		std::vector<RegisteredProvider>  g_providers;
		std::uint32_t                    g_nextToken = 1;

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

	std::uint32_t RegisterOutcomeProvider(OSFDefeatAPI::OutcomeProvider a_provider, void* a_userData)
	{
		if (!a_provider) {
			return 0;
		}
		std::scoped_lock lk{ g_providersMutex };
		const std::uint32_t token = g_nextToken++;
		g_providers.push_back({ token, a_provider, a_userData });
		REX::INFO("[OSFDefeat] outcome provider registered (token {})", token);
		return token;
	}

	bool UnregisterOutcomeProvider(std::uint32_t a_token)
	{
		std::scoped_lock lk{ g_providersMutex };
		const auto removed = std::erase_if(g_providers,
			[a_token](const RegisteredProvider& a_p) { return a_p.token == a_token; });
		return removed > 0;
	}

	Outcome SelectOutcome(
		EncounterClass a_class, const RE::Actor* a_victim, const RE::Actor* a_aggressor)
	{
		const Outcome frameworkDefault{ /*hold*/ false, /*recoverySeconds*/ -1.0f };
		if (!a_victim) {
			return frameworkDefault;
		}
		const OSFDefeatAPI::DefeatContext ctx{
			a_victim->GetFormID(),
			a_aggressor ? a_aggressor->GetFormID() : 0u,
			static_cast<std::uint32_t>(a_class),
		};

		// 1. Registered C++ providers (explicit integrations win). Copy the list under the lock,
		//    then invoke unlocked so a provider may do anything (incl. (un)register) without
		//    deadlocking; the first non-kDefault verdict wins.
		std::vector<RegisteredProvider> providers;
		{
			std::scoped_lock lk{ g_providersMutex };
			providers = g_providers;
		}
		for (const auto& provider : providers) {
			if (const auto outcome = VerdictToOutcome(provider.fn(ctx, provider.userData))) {
				return *outcome;
			}
		}

		// 2. Data-driven rules (the authored content/config layer).
		if (const auto verdict = Consequence::Evaluate(ctx)) {
			if (const auto outcome = VerdictToOutcome(*verdict)) {
				return *outcome;
			}
		}

		// 3. Built-in framework default: auto-recover after the default beat.
		return frameworkDefault;
	}
}
