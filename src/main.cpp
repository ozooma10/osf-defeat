#include "pch.h"

#include "Defeat.h"

namespace
{
	void InstallTask()
	{
		const auto* const tasks = SFSE::GetTaskInterface();
		if (!tasks) {
			REX::WARN("[OSFDefeat] no SFSE TaskInterface - defeat disabled");
			return;
		}
		static bool installed = false;
		if (!installed) {
			installed = true;
			tasks->AddPermanentTask([] { OSFDefeat::OnGameFrame(); });
			REX::INFO("[OSFDefeat] per-frame defeat task installed");
		}
	}

	void OnMessage(SFSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg && a_msg->type == SFSE::MessagingInterface::kPostPostDataLoad) {
			InstallTask();
		}
	}
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse);  // hook-free: no trampoline needed

	if (const auto* const messaging = SFSE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	}

	REX::INFO("OSF Defeat loaded");
	return true;
}
