#include "pch.h"

#include "Core/Config.h"
#include "Core/Consequence.h"
#include "Core/Events.h"
#include "Core/Persistence.h"
#include "Core/PlayerDefeat.h"
#include "Core/Presentation.h"
#include "Core/Reattach.h"
#include "Papyrus/Functions.h"

namespace
{
    void OnMessage(SFSE::MessagingInterface::Message* a_msg)
    {
        if (!a_msg) {
            return;
        }

        if (a_msg->type == SFSE::MessagingInterface::kPostDataLoad) {
            OSFDefeat::Config::Load();
            OSFDefeat::Consequence::Load();
            OSFDefeat::Events::Install();
            OSFDefeat::PlayerDefeat::Install();
            OSFDefeat::Reattach::Install();
            OSFDefeat::Papyrus::Register();
        } else if (a_msg->type == SFSE::MessagingInterface::kPostPostDataLoad) {
            // OSF Animation marks its native scene API ready at kPostDataLoad; fetch + cache it the
            // phase after, to dodge the intra-phase race (a too-early fetch is a safe null + retry).
            OSFDefeat::Presentation::AcquireSceneApi();
        }
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
    SFSE::Init(a_sfse, { .trampoline = true, .trampolineSize = 1024 });

    OSFDefeat::Persistence::Register();

    if (const auto* const messaging = SFSE::GetMessagingInterface()) {
        messaging->RegisterListener(OnMessage);
    }

    REX::INFO("OSF Defeat loaded");
    return true;
}