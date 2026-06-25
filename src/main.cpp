#include "pch.h"

#include "API/APIServer.h"
#include "API/OSFDefeatAPI.h"
#include "Core/Persistence.h"
#include "Core/Pipeline.h"
#include "Harness/NpcProbe.h"
#include "Trigger/PollTrigger.h"

// =============================================================================
// OSF Defeat — native defeat / death-alternative *framework* for Starfield.
//
// Layered core (see each module's header):
//   Engine        low-level RE'd primitives (combat-AI gate, AV owner, essential)
//   Trigger       pluggable down-detection — PollTrigger today, hooks later
//   DefeatRegistry thread-safe store of defeated actors (the heart)
//   Defeat        actor-agnostic verbs (defeat / rescue / pacify)
//   Validation    the gate (CanProcess / ValidatePair)
//   Pipeline      orchestration: detect -> validate -> classify -> apply -> outcome
//   Persistence   co-save serialization of the registry
//   API           public versioned interface + event broadcasts (the framework seam)
//
// Hook-free for now: the only engine writes are the player's Essential flag and
// the global combat-AI gate, driven from a per-frame SFSE task.
// =============================================================================
namespace
{
	std::unique_ptr<OSFDefeat::Trigger::ITrigger> g_trigger;

	void InstallCore()
	{
		if (g_trigger) {
			return;
		}
		auto trigger = std::make_unique<OSFDefeat::Trigger::PollTrigger>();
		trigger->SetSink([](const OSFDefeat::Trigger::DownEvent& a_event) {
			OSFDefeat::Pipeline::OnDown(a_event);
		});
		trigger->Install();
		g_trigger = std::move(trigger);

		OSFDefeat::API::Server::Register();
		OSFDefeat::Harness::NpcProbe::Install();  // diagnostic: validates NPC instance-essential (1)/(2)
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
	SFSE::Init(a_sfse);  // hook-free: no trampoline needed (yet)

	OSFDefeat::Persistence::Register();

	if (const auto* const messaging = SFSE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}

	REX::INFO("OSF Defeat loaded");
	return true;
}
