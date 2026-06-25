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
// =============================================================================

#include <cstdint>

namespace RE
{
	class Actor;
}

namespace OSFDefeatAPI
{
	// SFSE message ids (4cc). Distinct across request/response/events.
	inline constexpr std::uint32_t kRequestMessage = 'OSFr';   // consumer -> OSF: send me the interface
	inline constexpr std::uint32_t kResponseMessage = 'OSFR';  // OSF -> consumer: here is the interface (data = IVDefeat1**)
	inline constexpr std::uint32_t kOnDefeatMessage = 'OSFd';  // OSF -> all: an actor was defeated (data = DefeatEvent*)
	inline constexpr std::uint32_t kOnRescueMessage = 'OSFs';  // OSF -> all: an actor was rescued (data = DefeatEvent*)

	inline constexpr std::uint32_t kInterfaceVersion = 1;

	// Payload broadcast with kOnDefeatMessage / kOnRescueMessage.
	struct DefeatEvent
	{
		std::uint32_t victim;     // FormID of the defeated/rescued actor
		std::uint32_t aggressor;  // FormID of the aggressor, or 0 if unknown
	};

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
}
