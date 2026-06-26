#include "pch.h"

#include "API/APIServer.h"
#include "API/OSFDefeatAPI.h"
#include "Core/Persistence.h"
#include "Core/Pipeline.h"
#include "Harness/NpcProbe.h"
#include "Trigger/HookTrigger.h"
#include "Trigger/PollTrigger.h"

// =============================================================================
// OSF Defeat — native defeat / death-alternative *framework* for Starfield.
//
// Layered core (see each module's header):
//   Engine        low-level RE'd primitives (combat-AI gate, AV owner, essential) + the hook
//   Trigger       pluggable down-detection — PollTrigger (player) + HookTrigger (NPC)
//   DefeatRegistry thread-safe store of defeated actors (the heart)
//   Defeat        actor-agnostic verbs (defeat / rescue / pacify)
//   Validation    the gate (CanProcess / ValidatePair)
//   Pipeline      orchestration: detect -> validate -> classify -> apply -> outcome
//   Persistence   co-save serialization of the registry
//   API           public versioned interface + event broadcasts (the framework seam)
//
// One hook: HookTrigger entry-hooks ActorValueOwner::ModActorValue (AddrLib ID 101587)
// to clamp would-be-lethal NPC damage pre-death. The trampoline is allocated in
// SFSE::Init below; everything else is poll/flag-driven from per-frame SFSE tasks.
// =============================================================================
namespace
{
	std::vector<std::unique_ptr<OSFDefeat::Trigger::ITrigger>> g_triggers;

	void InstallTrigger(std::unique_ptr<OSFDefeat::Trigger::ITrigger> a_trigger)
	{
		a_trigger->SetSink([](const OSFDefeat::Trigger::DownEvent& a_event) {
			OSFDefeat::Pipeline::OnDown(a_event);
		});
		a_trigger->Install();
		g_triggers.push_back(std::move(a_trigger));
	}

	void InstallCore()
	{
		if (!g_triggers.empty()) {
			return;
		}

		// Both triggers compose on the same sink (Pipeline::OnDown is victim-keyed +
		// idempotent via the registry):
		//   PollTrigger — the player path (base Essential -> engine bleedout -> poll Health<=0).
		//   HookTrigger — the NPC path (pre-death ModActorValue damage clamp -> attributed down);
		//                 catches NPCs that die in one frame with no bleedout window to poll.
		// PollTrigger stays installed as the player fallback — do NOT regress it. To run
		// poll-only (e.g. if the hook ever drifts on a game update), drop the HookTrigger line.
		InstallTrigger(std::make_unique<OSFDefeat::Trigger::PollTrigger>());
		InstallTrigger(std::make_unique<OSFDefeat::Trigger::HookTrigger>());

		OSFDefeat::Pipeline::Install();  // game-thread, game-time recovery tick (drains auto-recoveries)

		OSFDefeat::API::Server::Register();
		OSFDefeat::Harness::NpcProbe::Install();  // diagnostic, default-off
		REX::INFO("[OSFDefeat] core installed");
	}

	void OnMessage(SFSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}
		if (a_msg->type == SFSE::MessagingInterface::kPostPostDataLoad) {
			InstallCore();
		} else if (a_msg->type == OSFDefeatAPI::kRequestMessage) {
			OSFDefeat::API::Server::GetSingleton()->OnRequest(a_msg);
		}
	}
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
	// Trampoline allocated for HookTrigger's single entry hook (ModActorValue, ID 101587):
	// the gateway (stolen prologue + jmp-back) plus the out-of-range branch stub fit easily.
	SFSE::Init(a_sfse, { .trampoline = true, .trampolineSize = 256 });

	OSFDefeat::Persistence::Register();

	if (const auto* const messaging = SFSE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}

	REX::INFO("OSF Defeat loaded");
	return true;
}
