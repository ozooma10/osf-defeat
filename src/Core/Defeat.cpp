#include "pch.h"

#include "Core/Defeat.h"

#include "Core/Capture.h"
#include "Core/Events.h"
#include "Core/Presentation.h"
#include "Core/RuntimeMutation.h"
#include "Engine/Engine.h"

namespace OSFDefeat::Defeat
{
    namespace
    {
        std::map<FormID, bool> g_aiSuspensions;  // actor ID -> AI-enabled state before OSF suspended it
        std::shared_mutex g_aiSuspensionsLock;

        [[nodiscard]] float NowDay()
        {
            const auto* const cal = RE::Calendar::GetSingleton();
            return cal ? cal->GetDaysPassedExact() : 0.0f;
        }
    }

    bool DefeatActor(RE::Actor* a_victim, RE::Actor* a_aggressor, DefeatCause a_cause)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
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
        Engine::ApplyDownState(*a_victim, *data);
        REX::INFO("[OSFDefeat] registered defeated actor {:08X} (aggressor {:08X})", id, aggressorId);

        if (a_victim->IsPlayerRef()) {
            Presentation::ShowPlayerDown();
            Events::BroadcastPlayerDefeated();
        }
        Events::BroadcastActorDefeated(id, aggressorId);
        return true;
    }

    bool RecoverActor(RE::Actor* a_actor, bool a_resumeAI)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_actor) {
            return false;
        }

        const FormID id = a_actor->GetFormID();
        auto data = DefeatRegistry::Get(id);
        if (!data) {
            return false;
        }

        const FormID aggressorId = data->aggressor;
        // Remove under the mutation lock before engine calls or notifications can re-enter.
        // Repeated recovery then finds no active record and has nothing to restore twice.
        DefeatRegistry::Remove(id);
        Engine::RestoreState(*a_actor, *data);
        if (a_actor->IsPlayerRef()) {
            Presentation::ClearPlayerDown();  // stop the cower + release the lock + restore camera
        }
        if (a_resumeAI) {
            ResumeAI(a_actor);
        }

        Capture::NotifyRecovered(id);  // a recovered actor is no longer a captive

        REX::INFO("[OSFDefeat] recovered actor {:08X}", id);
        if (a_actor->IsPlayerRef()) {
            Events::BroadcastPlayerRecovered();
        }
        Events::BroadcastActorRecovered(id, aggressorId);
        return true;
    }

    std::size_t ForceRecoverAll()
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        std::size_t recovered = 0;
        for (const auto id : DefeatRegistry::Snapshot()) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                if (RecoverActor(actor, true)) {
                    ++recovered;
                }
            } else {
                DefeatRegistry::Remove(id);  // unresolvable: purge so the co-save empties anyway
            }
        }

        // Resume AI for actors that were never registry entries (legacy PacifyActor without defeat).
        std::vector<FormID> suspendedActorIds;
        {
            std::shared_lock lock{ g_aiSuspensionsLock };
            suspendedActorIds.reserve(g_aiSuspensions.size());
            for (const auto& [id, _] : g_aiSuspensions) {
                suspendedActorIds.push_back(id);
            }
        }
        std::size_t resumedAI = 0;
        for (const auto id : suspendedActorIds) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                ResumeAI(actor);
                ++resumedAI;
            }
        }

        // Strip captive-faction memberships we created but recovery didn't cover (content may have
        // applied them to actors that were never defeated). Unresolvable actors keep their tracking
        // entry so a later load can still clean the membership.
        for (const auto& [actorId, factionId] : Engine::SnapshotCaptiveFactions()) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                Engine::RemoveCaptiveFaction(*actor);
            }
        }

        // Recovering every defeated actor above already cleared their capture relationships via
        // NotifyRecovered; drop anything left so the co-save's capture block empties too.
        Capture::Clear();

        const auto residualAISuspensions = SnapshotAISuspensions().size();
        const auto residualFactions = Engine::SnapshotCaptiveFactions().size();
        REX::INFO("[OSFDefeat] ForceRecoverAll: recovered {} actor(s), resumed AI for {} actor(s); {} AI suspension and {} captive-faction record(s) remain unresolved",
            recovered, resumedAI, residualAISuspensions, residualFactions);
        return recovered;
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

    void RestoreFromSave(FormID a_id, float a_registeredDay, FormID a_aggressor, float a_autoRecoverAtDay,
        float a_leaseExpiresAtDay, std::string a_leaseHolderId, DefeatCause a_cause, ActorStateSnapshot a_priorState)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        auto data = DefeatRegistry::Add(a_id, a_registeredDay, a_aggressor, a_autoRecoverAtDay);
        if (data) {
            data->leaseExpiresAtDay = a_leaseExpiresAtDay;
            data->SetLeaseHolderId(std::move(a_leaseHolderId));
            data->cause = a_cause;
            data->priorState = a_priorState;
            // Re-apply down-state on load. The flag writes are safe on an unloaded actor; the Set3D
            // hook re-asserts again once its 3D (re)builds, so this covers both loaded and unloaded.
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(a_id)) {
                Engine::ApplyDownState(*actor, *data);
            }
        }
    }

    void SuspendAI(RE::Actor* a_victim)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_victim) {
            return;
        }
        bool inserted = false;
        {
            std::unique_lock lock{ g_aiSuspensionsLock };
            const bool wasAIEnabled = a_victim->boolBits.all(RE::Actor::BOOL_BITS::kProcessMe);
            inserted = g_aiSuspensions.emplace(a_victim->GetFormID(), wasAIEnabled).second;
        }
        // Per-actor AI suppression — the `tai`/kProcessMe primitive (OSF RE, ID 101266). Halts the
        // actor's own AI (useful to hold a captive) at the cost of freezing its pose. Never the player.
        if (inserted && !a_victim->IsPlayerRef()) {
            a_victim->SetAIEnabled(false);
        }
    }

    void ResumeAI(RE::Actor* a_victim)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_victim) {
            return;
        }
        std::optional<bool> wasAIEnabled;
        {
            std::unique_lock lock{ g_aiSuspensionsLock };
            if (const auto it = g_aiSuspensions.find(a_victim->GetFormID()); it != g_aiSuspensions.end()) {
                wasAIEnabled = it->second;
                g_aiSuspensions.erase(it);
            }
        }
        if (wasAIEnabled.has_value() && !a_victim->IsPlayerRef()) {
            a_victim->SetAIEnabled(*wasAIEnabled);
        }
    }

    bool IsAISuspended(const RE::Actor* a_victim)
    {
        if (!a_victim) {
            return false;
        }
        std::shared_lock lock{ g_aiSuspensionsLock };
        return g_aiSuspensions.contains(a_victim->GetFormID());
    }

    std::vector<std::pair<FormID, bool>> SnapshotAISuspensions()
    {
        std::shared_lock lock{ g_aiSuspensionsLock };
        return { g_aiSuspensions.begin(), g_aiSuspensions.end() };
    }

    void RestoreAISuspension(FormID a_id, bool a_wasAIEnabled)
    {
        if (!a_id) {
            return;
        }
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        std::unique_lock lock{ g_aiSuspensionsLock };
        g_aiSuspensions.try_emplace(a_id, a_wasAIEnabled);
    }

    void ResetRuntimeState()
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        DefeatRegistry::Clear();
        std::unique_lock lock{ g_aiSuspensionsLock };
        g_aiSuspensions.clear();
    }

}
