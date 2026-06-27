#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Events
{
    void RegisterPlayerDefeated(RE::TESForm* a_receiver);
    void UnregisterPlayerDefeated(RE::TESForm* a_receiver);
    void RegisterPlayerRecovered(RE::TESForm* a_receiver);
    void UnregisterPlayerRecovered(RE::TESForm* a_receiver);
    void RegisterPlayerConsequenceStarted(RE::TESForm* a_receiver);
    void UnregisterPlayerConsequenceStarted(RE::TESForm* a_receiver);

    void BroadcastPlayerDefeated();
    void BroadcastPlayerRecovered();
    void BroadcastPlayerConsequenceStarted();
    void Clear();
}