#pragma once

// =============================================================================
// OSF Defeat — PUBLIC framework API (ship this header to other mod authors)
//
// This header is intentionally standalone (no CommonLibSF/SFSE dependency beyond
// a forward declaration) so consumers can drop it in without linking anything.
//
// Two integration surfaces:
//   1. Interface request  — ask OSF Defeat for its versioned C++ interface.
//   2. Broadcast messages — listen for defeat/rescue notifications.
//
// Both go over the SFSE messaging interface.
//
// --- Requesting the interface ------------------------------------------------
//   // 1. register a listener for messages from "OSF Defeat"
//   SFSE::GetMessagingInterface()->RegisterListener("OSF Defeat", OnOSFMessage);
//   // 2. ask for the interface
//   SFSE::GetMessagingInterface()->Dispatch(
//       OSFDefeatAPI::kRequestMessage, nullptr, 0, "OSF Defeat");
//   // 3. in OnOSFMessage, capture it:
//   if (msg->type == OSFDefeatAPI::kResponseMessage)
//       g_osf = *reinterpret_cast<OSFDefeatAPI::IVDefeat1**>(msg->data);
//
// --- Listening for events ----------------------------------------------------
//   if (msg->type == OSFDefeatAPI::kOnDefeatMessage) {
//       auto* e = reinterpret_cast<OSFDefeatAPI::DefeatEvent*>(msg->data);
//       // e->victim, e->aggressor are FormIDs
//   }
//
// --- Authoring outcomes (interface v2+) --------------------------------------
//   // Cast up once you have the interface and confirmed its version:
//   if (g_osf->Version() >= 2) {
//       auto* v2 = static_cast<OSFDefeatAPI::IVDefeat2*>(g_osf);
//       v2->RegisterOutcomeProvider(&MyProvider, this);
//   }
//   // MyProvider runs on the game thread when an actor is defeated; return kHold to keep it
//   // down until you rescue it, kRelease to set a recovery time, or kDefault to defer.
// =============================================================================

#include <cstdint>

namespace RE
{
	class Actor;
}

namespace OSFDefeatAPI
{
	// SFSE message ids (4cc). Distinct across request/response/events.
	inline constexpr std::uint32_t kRequestMessage = 'OSFr';      // consumer -> OSF: send me the interface
	inline constexpr std::uint32_t kResponseMessage = 'OSFR';     // OSF -> consumer: here is the interface (data = IVDefeat1**)
	inline constexpr std::uint32_t kOnDefeatMessage = 'OSFd';     // OSF -> all: an actor was defeated (data = DefeatEvent*)
	inline constexpr std::uint32_t kOnRescueMessage = 'OSFs';     // OSF -> all: an actor was rescued (data = DefeatEvent*)
	inline constexpr std::uint32_t kOnConsequenceMessage = 'OSFc';  // OSF -> all: an authored consequence fired (data = ConsequenceEvent*)

	inline constexpr std::uint32_t kInterfaceVersion = 2;

	// Payload broadcast with kOnDefeatMessage / kOnRescueMessage.
	struct DefeatEvent
	{
		std::uint32_t victim;     // FormID of the defeated/rescued actor
		std::uint32_t aggressor;  // FormID of the aggressor, or 0 if unknown
	};

	// Payload broadcast with kOnConsequenceMessage: an authored "event" consequence from the data
	// config fired for a defeat. The framework declares WHAT (a tag); the listening mod implements
	// HOW (capture, ransom, loot, ...). This is the data-driven consequence seam.
	struct ConsequenceEvent
	{
		std::uint32_t victim;     // FormID of the defeated actor
		std::uint32_t aggressor;  // FormID of the aggressor, or 0 if unknown
		const char*   tag;        // null-terminated event tag from the rule; valid ONLY during this
								  // synchronous dispatch — copy it if you retain it past the callback
	};

	// Coarse encounter classification handed to an outcome provider. Mirrors the framework's
	// internal taxonomy; treat unrecognized values as kUnknown (more may be added later).
	enum class EncounterClass : std::uint32_t
	{
		kHostile = 0,   // an enemy combatant — the standard defeat
		kCrew = 1,      // the player's own teammate / commanded actor (normally excluded upstream)
		kSecurity = 2,  // a guard / law-enforcement actor
		kCivilian = 3,  // a non-combatant bystander (has a crime faction, not currently fighting)
		kUnknown = 4,
	};

	// Context passed to an outcome provider for one defeat.
	struct DefeatContext
	{
		std::uint32_t victim;          // FormID of the actor being defeated
		std::uint32_t aggressor;       // FormID of the aggressor, or 0 if unknown
		std::uint32_t encounterClass;  // an EncounterClass value
	};

	// A provider's verdict for a defeat.
	struct DefeatOutcome
	{
		enum class Disposition : std::uint32_t
		{
			kDefault = 0,  // decline — defer to the next provider, then the framework default
			kRelease = 1,  // auto-recover after recoverySeconds
			kHold = 2,     // stay down until an explicit RescueActor
		};

		Disposition disposition = Disposition::kDefault;
		// For kRelease: REAL seconds until auto-recovery (<0 = framework default beat). Converted
		// to game-time internally, so the wait pauses with the game and scales with sleep/wait.
		float recoverySeconds = -1.0f;
	};

	// Outcome provider callback. Invoked ON THE GAME THREAD when an actor is defeated, BEFORE the
	// recovery timer is set. Return kDefault to defer. `a_userData` is the pointer you registered.
	// Do not block, and do not (un)register providers from within the callback.
	using OutcomeProvider = DefeatOutcome (*)(const DefeatContext& a_context, void* a_userData);

	// Versioned C++ interface. Never reorder/remove methods in a published version;
	// add new methods to a new IVDefeat2 and bump kInterfaceVersion.
	class IVDefeat1
	{
	public:
		[[nodiscard]] virtual std::uint32_t Version() const noexcept = 0;

		// State-mutating calls are marshalled onto the game thread and return
		// immediately; the bool means "request accepted/queued", not "completed".
		virtual bool DefeatActor(RE::Actor* a_victim) = 0;
		virtual bool RescueActor(RE::Actor* a_victim, bool a_undoPacify) = 0;

		// Read-only; safe to call from any thread.
		[[nodiscard]] virtual bool IsDefeated(RE::Actor* a_victim) = 0;

	protected:
		~IVDefeat1() = default;
	};

	// Interface v2 — outcome authoring. The object returned by the interface request implements
	// the highest version it supports; check Version() and static_cast IVDefeat1* up to here.
	class IVDefeat2 : public IVDefeat1
	{
	public:
		// Register an outcome provider. Providers are consulted in registration order on each
		// defeat; the first non-kDefault verdict wins. Returns a non-zero token (0 = failure;
		// e.g. null provider) to pass to UnregisterOutcomeProvider. Thread-safe.
		virtual std::uint32_t RegisterOutcomeProvider(OutcomeProvider a_provider, void* a_userData) = 0;
		virtual bool          UnregisterOutcomeProvider(std::uint32_t a_token) = 0;

	protected:
		~IVDefeat2() = default;
	};
}
