#include "pch.h"

#include "Core/Defeat.h"

#include "Core/Events.h"
#include "Core/Presentation.h"
#include "Engine/Engine.h"

namespace OSFDefeat::Defeat
{
    namespace
    {
        Observer g_onDefeat;
        Observer g_onRescue;
        std::set<FormID> g_pacified;
        std::shared_mutex g_pacifiedLock;

        [[nodiscard]] float NowDay()
        {
            const auto* const cal = RE::Calendar::GetSingleton();
            return cal ? cal->GetDaysPassedExact() : 0.0f;
        }
    }

    void SetOnDefeat(Observer a_observer) { g_onDefeat = std::move(a_observer); }
    void SetOnRescue(Observer a_observer) { g_onRescue = std::move(a_observer); }

    bool DefeatActor(RE::Actor* a_victim, RE::Actor* a_aggressor)
    {
        if (!a_victim || a_victim->IsDead()) {
            return false;
        }

        const FormID id = a_victim->GetFormID();
        const FormID aggressorId = a_aggressor ? a_aggressor->GetFormID() : 0;
        auto data = DefeatRegistry::Add(id, NowDay(), aggressorId);
        if (!data) {
            return false;
        }

        data->state.store(DefeatState::Defeated);
        Engine::ApplyDownState(a_victim);
        REX::INFO("[OSFDefeat] registered defeated actor {:08X} (aggressor {:08X})", id, aggressorId);

        if (a_victim->IsPlayerRef()) {
            // The framework state holds the player safe; the cower pose + control lock are what make the
            // down VISIBLE (lifeState can't pose the player). Cleared on rescue below.
            Presentation::ShowPlayerDown();
            Events::BroadcastPlayerDefeated();
        }
        if (g_onDefeat) {
            g_onDefeat(id, aggressorId);
        }
        return true;
    }

    bool RescueActor(RE::Actor* a_victim, bool a_undoPacify)
    {
        if (!a_victim) {
            return false;
        }

        const FormID id = a_victim->GetFormID();
        auto data = DefeatRegistry::Get(id);
        if (!data) {
            return false;
        }

        auto expected = DefeatState::Defeated;
        if (!data->state.compare_exchange_strong(expected, DefeatState::Rescuing)) {
            return false;
        }

        const FormID aggressorId = data->aggressor;
        DefeatRegistry::Remove(id);
        Engine::RestoreState(a_victim);
        if (a_victim->IsPlayerRef()) {
            Presentation::ClearPlayerDown();  // stop the cower + release the lock + restore camera
        }
        if (a_undoPacify) {
            UndoPacify(a_victim);
        }

        REX::INFO("[OSFDefeat] rescued actor {:08X}", id);
        if (a_victim->IsPlayerRef()) {
            Events::BroadcastPlayerRecovered();
        }
        if (g_onRescue) {
            g_onRescue(id, aggressorId);
        }
        return true;
    }

    bool IsDefeated(const RE::Actor* a_victim)
    {
        return a_victim && DefeatRegistry::Contains(a_victim->GetFormID());
    }

    bool IsDamageImmune(const RE::Actor* a_victim)
    {
        return IsDefeated(a_victim);
    }

    std::vector<RE::Actor*> GetAllDefeated(bool /*a_loadedOnly*/)
    {
        std::vector<RE::Actor*> out;
        for (const auto id : DefeatRegistry::Snapshot()) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                out.push_back(actor);
            }
        }
        return out;
    }

    void RestoreFromSave(FormID a_id, float a_registeredDay, FormID a_aggressor, float a_recoverAtDay)
    {
        auto data = DefeatRegistry::Add(a_id, a_registeredDay, a_aggressor, a_recoverAtDay);
        if (data) {
            data->state.store(DefeatState::Defeated);
            // Re-apply down-state on load. The flag writes are safe on an unloaded actor; the Set3D
            // hook re-asserts again once its 3D (re)builds, so this covers both loaded and unloaded.
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(a_id)) {
                Engine::ApplyDownState(actor);
            }
        }
    }

    void Pacify(RE::Actor* a_victim)
    {
        if (!a_victim) {
            return;
        }
        {
            std::unique_lock lock{ g_pacifiedLock };
            g_pacified.insert(a_victim->GetFormID());
        }
        // Per-actor AI suppression — the `tai`/kProcessMe primitive (OSF RE, ID 101266). Halts the
        // actor's own AI (useful to hold a captive) at the cost of freezing its pose. Never the player.
        if (!a_victim->IsPlayerRef()) {
            a_victim->SetAIEnabled(false);
        }
    }

    void UndoPacify(RE::Actor* a_victim)
    {
        if (!a_victim) {
            return;
        }
        {
            std::unique_lock lock{ g_pacifiedLock };
            g_pacified.erase(a_victim->GetFormID());
        }
        if (!a_victim->IsPlayerRef()) {
            a_victim->SetAIEnabled(true);
        }
    }

    bool IsPacified(const RE::Actor* a_victim)
    {
        if (!a_victim) {
            return false;
        }
        std::shared_lock lock{ g_pacifiedLock };
        return g_pacified.contains(a_victim->GetFormID());
    }
}