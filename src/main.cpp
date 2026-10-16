#include "pch.h"

#include "Core/Config.h"
#include "Core/Consequence.h"
#include "Core/Decision.h"
#include "Core/Persistence.h"
#include "Core/PlayerDefeat.h"
#include "Core/Presentation.h"
#include "Core/Reattach.h"
#include "Core/Recovery.h"
#include "Papyrus/Functions.h"

namespace
{
    void OnMessage(SFSE::MessagingInterface::Message* a_msg)
    {
        if (!a_msg) {
            return;
        }

        if (a_msg->type == SFSE::MessagingInterface::kPostDataLoad) {
            OSFDefeat::Decision::Initialize();
            if (!OSFDefeat::Recovery::Install()) {
                REX::ERROR("[OSFDefeat] initialization stopped: recovery update is required before enabling defeat");
                return;
            }
            OSFDefeat::Persistence::Register();
            OSFDefeat::Config::Load();
            OSFDefeat::Consequence::Load();
            OSFDefeat::PlayerDefeat::Install();
            OSFDefeat::Reattach::Install();
            OSFDefeat::Papyrus::Register();
        } else if (a_msg->type == SFSE::MessagingInterface::kPostPostDataLoad) {
            // OSF Animation marks its native scene API ready at kPostDataLoad; cache it once in
            // the following phase, after its initialization phase completes.
            OSFDefeat::Presentation::AcquireSceneApi();
        }
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
    SFSE::Init(a_sfse, { .trampoline = true, .trampolineSize = 1024 });

    if (const auto* const messaging = SFSE::GetMessagingInterface()) {
        messaging->RegisterListener(OnMessage);
    }

    REX::INFO("OSF Defeat loaded");
    return true;
}
