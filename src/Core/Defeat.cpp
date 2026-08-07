#include "pch.h"

#include "Core/Defeat.h"

#include "Core/Capture.h"
#include "Core/Events.h"
#include "Core/Presentation.h"
#include "Core/RuntimeState.h"
#include "Engine/Engine.h"

namespace OSFDefeat::Defeat
{
    namespace
    {
        Observer g_onDefeat;
        Observer g_onRescue;
        std::map<FormID, bool> g_pacified;  // actor ID -> AI-enabled state before OSF pacified it
        std::shared_mutex g_pacifiedLock;

        [[nodiscard]] float NowDay()
        {
            const auto* const cal = RE::Calendar::GetSingleton();
            return cal ? cal->GetDaysPassedExact() : 0.0f;
        }
    }

    void SetOnDefeat(Observer a_observer) { g_onDefeat = std::move(a_observer); }
    void SetOnRescue(Observer a_observer) { g_onRescue = std::move(a_observer); }

    bool DefeatActor(RE::Actor* a_victim, RE::Actor* a_aggressor, DefeatCause a_cause)
    {
        std::scoped_lock stateLock{ RuntimeState::Mutex() };
        if (!a_victim || a_victim->IsDead()) {
            return false;
        }

        const FormID id = a_victim->GetFormID();
        const FormID aggressorId = a_aggressor ? a_aggressor->GetFormID() : 0;
        auto data = DefeatRegistry::Add(id, NowDay(), aggressorId);
        if (!data) {
            return false;
        }

        data->cause = a_cause;
        data->state.store(DefeatState::Defeated);
        Engine::ApplyDownState(a_victim, data);
        REX::INFO("[OSFDefeat] registered defeated actor {:08X} (aggressor {:08X})", id, aggressorId);

        if (a_victim->IsPlayerRef()) {
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
        std::scoped_lock stateLock{ RuntimeState::Mutex() };
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
        Engine::RestoreState(a_victim, data);
        if (a_victim->IsPlayerRef()) {
            Presentation::ClearPlayerDown();  // stop the cower + release the lock + restore camera
        }
        if (a_undoPacify) {
            UndoPacify(a_victim);
        }

        Capture::NotifyRecovered(id);  // a recovered actor is no longer a captive

        REX::INFO("[OSFDefeat] rescued actor {:08X}", id);
        if (a_victim->IsPlayerRef()) {
            Events::BroadcastPlayerRecovered();
        }
        if (g_onRescue) {
            g_onRescue(id, aggressorId);
        }
        return true;
    }

    std::size_t ForceRecoverAll()
    {
        std::scoped_lock stateLock{ RuntimeState::Mutex() };
        std::size_t rescued = 0;
        for (const auto id : DefeatRegistry::Snapshot()) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                if (RescueActor(actor, true)) {
                    ++rescued;
                }
            } else {
                DefeatRegistry::Remove(id);  // unresolvable: purge so the co-save empties anyway
            }
        }

        // Release pacified actors that were never registry entries (content PacifyActor without defeat).
        std::vector<FormID> pacified;
        {
            std::shared_lock lock{ g_pacifiedLock };
            pacified.reserve(g_pacified.size());
            for (const auto& [id, _] : g_pacified) {
                pacified.push_back(id);
            }
        }
        std::size_t releasedPacified = 0;
        for (const auto id : pacified) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                UndoPacify(actor);
                ++releasedPacified;
            }
        }

        // Strip captive-faction memberships we created but rescue didn't cover (content may have
        // applied them to actors that were never defeated). Unresolvable actors keep their tracking
        // entry so a later load can still clean the membership.
        for (const auto& [actorId, factionId] : Engine::SnapshotCaptiveFactions()) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                Engine::RemoveCaptiveFaction(actor);
            }
        }

        // Rescuing every defeated actor above already cleared their capture records via
        // NotifyRecovered; drop anything left so the co-save's capture block empties too.
        Capture::Clear();

        const auto residualPacified = SnapshotPacified().size();
        const auto residualFactions = Engine::SnapshotCaptiveFactions().size();
        REX::INFO("[OSFDefeat] ForceRecoverAll: rescued {} actor(s), released {} pacified actor(s); {} pacify and {} captive-faction record(s) remain unresolved",
            rescued, releasedPacified, residualPacified, residualFactions);
        return rescued;
    }

    bool IsDefeated(const RE::Actor* a_victim)
    {
        return a_victim && DefeatRegistry::Contains(a_victim->GetFormID());
    }

    DefeatCause GetDefeatCause(const RE::Actor* a_victim)
    {
        if (!a_victim) {
            return DefeatCause::kNone;
        }
        const auto data = DefeatRegistry::Get(a_victim->GetFormID());
        return data ? data->cause.load(std::memory_order_relaxed) : DefeatCause::kNone;
    }

    bool IsDamageImmune(const RE::Actor* a_victim)
    {
        return IsDefeated(a_victim);
    }

    std::vector<RE::Actor*> GetAllDefeated(bool a_loadedOnly)
    {
        std::vector<RE::Actor*> out;
        for (const auto id : DefeatRegistry::Snapshot()) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                if (a_loadedOnly) {
                    const auto loaded = actor->loadedData.LockRead();
                    if (!*loaded || !(*loaded)->data3D) {
                        continue;
                    }
                }
                out.push_back(actor);
            }
        }
        return out;
    }

    void RestoreFromSave(FormID a_id, float a_registeredDay, FormID a_aggressor, float a_recoverAtDay,
        float a_leaseDeadlineDay, std::string a_leaseOwner, DefeatCause a_cause, ActorStateSnapshot a_priorState)
    {
        std::scoped_lock stateLock{ RuntimeState::Mutex() };
        auto data = DefeatRegistry::Add(a_id, a_registeredDay, a_aggressor, a_recoverAtDay);
        if (data) {
            data->leaseDeadlineDay = a_leaseDeadlineDay;
            data->SetLeaseOwner(std::move(a_leaseOwner));
            data->cause = a_cause;
            data->priorState = a_priorState;
            data->state.store(DefeatState::Defeated);
            // Re-apply down-state on load. The flag writes are safe on an unloaded actor; the Set3D
            // hook re-asserts again once its 3D (re)builds, so this covers both loaded and unloaded.
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(a_id)) {
                Engine::ApplyDownState(actor, data);
            }
        }
    }

    void Pacify(RE::Actor* a_victim)
    {
        std::scoped_lock stateLock{ RuntimeState::Mutex() };
        if (!a_victim) {
            return;
        }
        bool inserted = false;
        {
            std::unique_lock lock{ g_pacifiedLock };
            const bool wasAIEnabled = a_victim->boolBits.all(RE::Actor::BOOL_BITS::kProcessMe);
            inserted = g_pacified.emplace(a_victim->GetFormID(), wasAIEnabled).second;
        }
        // Per-actor AI suppression — the `tai`/kProcessMe primitive (OSF RE, ID 101266). Halts the
        // actor's own AI (useful to hold a captive) at the cost of freezing its pose. Never the player.
        if (inserted && !a_victim->IsPlayerRef()) {
            a_victim->SetAIEnabled(false);
        }
    }

    void UndoPacify(RE::Actor* a_victim)
    {
        std::scoped_lock stateLock{ RuntimeState::Mutex() };
        if (!a_victim) {
            return;
        }
        std::optional<bool> wasAIEnabled;
        {
            std::unique_lock lock{ g_pacifiedLock };
            if (const auto it = g_pacified.find(a_victim->GetFormID()); it != g_pacified.end()) {
                wasAIEnabled = it->second;
                g_pacified.erase(it);
            }
        }
        if (wasAIEnabled.has_value() && !a_victim->IsPlayerRef()) {
            a_victim->SetAIEnabled(*wasAIEnabled);
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

    std::vector<std::pair<FormID, bool>> SnapshotPacified()
    {
        std::shared_lock lock{ g_pacifiedLock };
        return { g_pacified.begin(), g_pacified.end() };
    }

    void RestorePacified(FormID a_id, bool a_wasAIEnabled)
    {
        if (!a_id) {
            return;
        }
        std::scoped_lock stateLock{ RuntimeState::Mutex() };
        std::unique_lock lock{ g_pacifiedLock };
        g_pacified.try_emplace(a_id, a_wasAIEnabled);
    }

    void ResetRuntimeState()
    {
        std::scoped_lock stateLock{ RuntimeState::Mutex() };
        DefeatRegistry::Clear();
        std::unique_lock lock{ g_pacifiedLock };
        g_pacified.clear();
    }
}
