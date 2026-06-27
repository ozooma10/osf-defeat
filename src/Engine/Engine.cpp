#include "pch.h"

#include "Engine/Engine.h"

#include "RE/C/Console.h"
#include "RE/P/ProcessLists.h"

namespace OSFDefeat::Engine
{
    namespace
    {
        std::atomic_bool g_hasEssentialSnapshot{ false };
        bool g_wasEssential{ false };

        [[nodiscard]] bool PlayerIsEssential(RE::Actor* a_actor)
        {
            auto* const npc = a_actor ? a_actor->GetNPC() : nullptr;
            return npc ? npc->IsEssential() : false;
        }
    }
    RE::ActorValueInfo* HealthAV()
    {
        const auto* const avs = RE::ActorValue::GetSingleton();
        return avs ? avs->health : nullptr;
    }

    RE::ActorValueOwner* AVOwnerOf(RE::Actor* a_actor)
    {
        return a_actor ? static_cast<RE::ActorValueOwner*>(static_cast<RE::TESObjectREFR*>(a_actor)) : nullptr;
    }

    float GetHealth(RE::Actor* a_actor)
    {
        auto* const av = HealthAV();
        auto* const owner = AVOwnerOf(a_actor);
        return av && owner ? owner->GetActorValue(*av) : 0.0f;
    }

    float GetMaxHealth(RE::Actor* a_actor)
    {
        auto* const av = HealthAV();
        auto* const owner = AVOwnerOf(a_actor);
        return av && owner ? owner->GetPermanentActorValue(*av) : 0.0f;
    }

    void RestoreHealthToMax(RE::Actor* a_actor)
    {
        auto* const av = HealthAV();
        auto* const owner = AVOwnerOf(a_actor);
        if (!av || !owner) {
            return;
        }

        const float maxHealth = std::max(owner->GetPermanentActorValue(*av), 1.0f);
        const float current = owner->GetActorValue(*av);
        if (current < maxHealth) {
            owner->RestoreActorValue(*av, maxHealth - current);
        }
        if (owner->GetActorValue(*av) < 1.0f) {
            owner->SetActorValue(*av, maxHealth);
        }
    }

    void SetCombatAIDisabled(bool a_disabled)
    {
        RE::Console::SetCombatAIProcessing(!a_disabled);
    }

    void SetPlayerEssential(bool a_essential)
    {
        auto* const player = RE::PlayerCharacter::GetSingleton();
        auto* const actor = static_cast<RE::Actor*>(player);
        auto* const npc = actor ? actor->GetNPC() : nullptr;
        if (!actor || !npc) {
            return;
        }

        actor->boolFlags.set(a_essential, RE::Actor::BOOL_FLAGS::kEssential);
        npc->actorData.actorBaseFlags.set(a_essential, RE::ACTOR_BASE_DATA::Flag::kEssential);
    }

    void SetPlayerDownState(RE::Actor* a_actor)
    {
        if (!a_actor || !a_actor->IsPlayerRef()) {
            return;
        }

        bool expected = false;
        if (g_hasEssentialSnapshot.compare_exchange_strong(expected, true)) {
            g_wasEssential = PlayerIsEssential(a_actor);
        }
        SetPlayerEssential(true);
        RestoreHealthToMax(a_actor);
        a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
        a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kCanSpeakToEssentialDown);
        a_actor->SetGhost(true);
        a_actor->SetAIEnabled(false);
        a_actor->SetLifeState(kBleedOut);
        a_actor->StopCombat();
        if (auto* const lists = RE::ProcessLists::GetSingleton()) {
            lists->StopCombatAndAlarmOnActor(a_actor);
        }
        SetCombatAIDisabled(true);
    }

    void RestorePlayerState(RE::Actor* a_actor)
    {
        if (!a_actor || !a_actor->IsPlayerRef()) {
            return;
        }

        RestoreHealthToMax(a_actor);
        a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
        a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kCanSpeakToEssentialDown);
        a_actor->SetGhost(false);
        a_actor->SetAIEnabled(true);
        a_actor->SetLifeState(kAlive);
        a_actor->EvaluatePackage(true, true);
        if (g_hasEssentialSnapshot.exchange(false)) {
            SetPlayerEssential(g_wasEssential);
        }
        SetCombatAIDisabled(false);
    }
}