#include "pch.h"

#include "Core/Persistence.h"
#include "Papyrus/Functions.h"

namespace
{
	void OnMessage(SFSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}

		if (a_msg->type == SFSE::MessagingInterface::kPostDataLoad) {
			OSFDefeat::Papyrus::Register();
		}
	}
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse);

	OSFDefeat::Persistence::Register();

	if (const auto* const messaging = SFSE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}

	REX::INFO("OSF Defeat loaded");
	return true;
}
