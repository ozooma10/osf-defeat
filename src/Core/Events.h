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

    void RegisterActorDefeated(RE::TESForm* a_receiver);
    void UnregisterActorDefeated(RE::TESForm* a_receiver);
    void RegisterActorRecovered(RE::TESForm* a_receiver);
    void UnregisterActorRecovered(RE::TESForm* a_receiver);

    void BroadcastPlayerDefeated();
    void BroadcastPlayerRecovered();
    void BroadcastPlayerConsequenceStarted();
    void BroadcastActorDefeated(FormID a_victim, FormID a_aggressor);
    void BroadcastActorRecovered(FormID a_victim, FormID a_aggressor);

    // Wires the dormant Defeat observers to the actor broadcasts. Call once at init.
    void Install();
    void Clear();
}