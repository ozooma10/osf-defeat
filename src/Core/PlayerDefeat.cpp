#include "pch.h"

#include "Core/PlayerDefeat.h"

#include "Core/Config.h"
#include "Core/Consequence.h"
#include "Core/Decision.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/Events.h"
#include "Core/DefeatRegistry.h"
#include "Core/QuestHandoff.h"
#include "Core/Validation.h"
#include "Engine/Engine.h"

#include "REL/ASM.h"
#include "REL/Utility.h"

namespace OSFDefeat::PlayerDefeat
{
    namespace
    {
        using ModActorValue_t = void(RE::ActorValueOwner*, RE::ACTOR_VALUE_MODIFIER, const RE::ActorValueInfo&, float, RE::TESObjectREFR*);

        constexpr REL::ID kModActorValueID{ 101587 };
        constexpr std::array<std::uint8_t, 9> kExpectedPrologue{ 0x48, 0x89, 0x5C, 0x24, 0x08, 0x89, 0x54, 0x24, 0x10 };
        constexpr std::size_t kStolenLength = kExpectedPrologue.size();
        constexpr float kLethalHealthFloor = 5.0f;

        ModActorValue_t* g_original = nullptr;
        std::atomic_bool g_hooked{ false };
        std::atomic_bool g_pendingDefeat{ false };
        std::atomic_bool g_workerStarted{ false };

        // --- Participant recording -----------------------------------------------------------------
        // Who damaged the player recently, fed passively by the hook below. This is the participant
        // source for the defeat-time snapshot: live ProcessLists actor arrays must NOT be iterated from
        // an SFSE task (proven crash: races the sim thread's realloc → AV in GetSmartPointer), so we
        // only store PODs here (FormID + timestamp, worker thread) and resolve via LookupByID on the
        // game thread later. "Damaged the player in the last window" is also the better cast list —
        // actual combatants, not bystanders.
        constexpr auto kParticipantWindow = std::chrono::seconds{ 30 };
        constexpr std::size_t kParticipantCap = 64;
        std::map<FormID, std::chrono::steady_clock::time_point> g_recentAttackers;
        std::mutex g_recentAttackersLock;

        void RecordAttacker(RE::TESObjectREFR* a_ref)
        {
            if (!a_ref) {
                return;
            }
            const FormID id = a_ref->GetFormID();
            const auto now = std::chrono::steady_clock::now();
            std::unique_lock lock{ g_recentAttackersLock };
            std::erase_if(g_recentAttackers, [&](const auto& entry) { return now - entry.second > kParticipantWindow; });
            if (g_recentAttackers.size() < kParticipantCap || g_recentAttackers.contains(id)) {
                g_recentAttackers[id] = now;
            }
        }

        [[nodiscard]] std::vector<FormID> RecentAttackerIDs()
        {
            const auto now = std::chrono::steady_clock::now();
            std::vector<FormID> out;
            std::unique_lock lock{ g_recentAttackersLock };
            for (const auto& [id, when] : g_recentAttackers) {
                if (now - when <= kParticipantWindow) {
                    out.push_back(id);
                }
            }
            return out;
        }

        // Defeat-time participant snapshot: the aggressor plus every recent attacker that is still a
        // live hostile within a_radius (<= 0 = no radius filter). Game thread (LookupByID + engine
        // reads). Recorded attackers that resolve to non-actors (traps, hazards) drop out here.
        [[nodiscard]] std::vector<RE::Actor*> CollectParticipants(RE::Actor* a_player, RE::Actor* a_aggressor, float a_radius)
        {
            std::vector<RE::Actor*> out;
            if (!a_player) {
                return out;
            }

            const auto origin = a_player->GetPosition();
            const float radiusSq = a_radius > 0.0f ? a_radius * a_radius : 0.0f;
            const auto consider = [&](RE::Actor* a_actor, bool a_requireHostile) {
                if (!a_actor || a_actor == a_player || a_actor->IsDead()) {
                    return;
                }
                if (std::find(out.begin(), out.end(), a_actor) != out.end()) {
                    return;
                }
                if (a_requireHostile && !a_actor->IsHostileToActor(a_player)) {
                    return;
                }
                if (radiusSq > 0.0f && (a_actor->GetPosition() - origin).SqrLength() > radiusSq) {
                    return;
                }
                out.push_back(a_actor);
            };

            // The aggressor is always cast (no hostility re-check: the captive-faction pacify or the
            // target-list drop may already have cleared formal hostility by the time content asks).
            consider(a_aggressor, false);
            for (const auto id : RecentAttackerIDs()) {
                consider(RE::TESForm::LookupByID<RE::Actor>(id), true);
            }
            return out;
        }

        [[nodiscard]] float NowDay()
        {
            const auto* const cal = RE::Calendar::GetSingleton();
            return cal ? cal->GetDaysPassedExact() : 0.0f;
        }

        [[nodiscard]] float TimeScale()
        {
            const auto* const cal = RE::Calendar::GetSingleton();
            return cal ? std::max(1.0f, cal->GetTimeScale()) : 20.0f;
        }

        [[nodiscard]] RE::Actor* PlayerActor()
        {
            return static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton());
        }

        [[nodiscard]] bool IsPlayerAVOwner(RE::ActorValueOwner* a_owner)
        {
            auto* const player = PlayerActor();
            return player && a_owner == Engine::AVOwnerOf(player);
        }

        void QueueBegin(RE::TESObjectREFR* a_ref)
        {
            bool expected = false;
            if (!g_pendingDefeat.compare_exchange_strong(expected, true)) {
                return;
            }

            const FormID aggressorId = a_ref ? a_ref->GetFormID() : 0;
            if (const auto* const tasks = SFSE::GetTaskInterface()) {
                tasks->AddTask([aggressorId]() {
                    auto* aggressor = aggressorId ? RE::TESForm::LookupByID<RE::Actor>(aggressorId) : nullptr;
                    // The slot-7 wrapper (Papyrus/console DamageValue) passes a_ref=null, so there is
                    // no direct dealer. Fall back to who the player was fighting (currentCombatTarget),
                    // resolved here on the game thread. NOT myKiller — that is only written once a kill
                    // commits, which the clamp prevents (disproven in-game; see HookTrigger.md).
                    if (!aggressor) {
                        aggressor = Engine::ResolveCombatTarget(PlayerActor());
                    }
                    BeginPlayerDefeat(aggressor);
                    g_pendingDefeat.store(false);
                });
            } else {
                g_pendingDefeat.store(false);
            }
        }

        // Both AVs reach a terminal state through this same ModActorValue primitive: Health -> death,
        // Electromagnetic Health -> the vanilla EM shock/incapacitation (OSF RE em-stun-damage-routing,
        // 2026-06-25). We clamp both identically so neither terminal path fires.
        [[nodiscard]] bool IsLethalGauge(const RE::ActorValueInfo& a_info)
        {
            return &a_info == Engine::HealthAV() || &a_info == Engine::EMHealthAV();
        }

        // Health keeps its flat floor (unchanged behavior). The EM Health gauge can be far smaller, so
        // cap the floor below its max — em-stun-damage-routing flagged a flat 5.0 as too high for a tiny
        // EM gauge. Falls back to the flat floor if the gauge max is unavailable.
        [[nodiscard]] float LethalFloorFor(RE::ActorValueOwner* a_owner, const RE::ActorValueInfo& a_info)
        {
            if (&a_info == Engine::HealthAV()) {
                return kLethalHealthFloor;
            }
            const float maxValue = a_owner ? a_owner->GetPermanentActorValue(a_info) : 0.0f;
            if (maxValue <= 0.0f) {
                return kLethalHealthFloor;
            }
            return std::min(kLethalHealthFloor, std::max(1.0f, maxValue * 0.05f));
        }

        // --- NPC auto-defeat (opt-in) ------------------------------------------------------------
        // Symmetric with the player clamp, just resolving the victim from the hooked AV owner and gating
        // on hostility/flesh. Classification (IsHostileToActor/HasKeyword) runs on the damage worker
        // thread — confirmed safe in the OSF RE sandbox (2026-06-27: 3821 concurrent worker-thread calls
        // across 22 threads, zero faults; game-thread cross-check consistent).

        // Reverse the AVOwnerOf cast: ActorValueOwner is a fixed-offset base of TESObjectREFR, so recover
        // the actor by subtracting that (constant) subobject offset, then RTTI-filter to an Actor.
        [[nodiscard]] RE::Actor* ActorFromAVOwner(RE::ActorValueOwner* a_owner)
        {
            if (!a_owner) {
                return nullptr;
            }
            static const std::ptrdiff_t avOffset = []() {
                auto* const fake = reinterpret_cast<RE::TESObjectREFR*>(std::uintptr_t{ 0x1000 });
                return reinterpret_cast<std::uint8_t*>(static_cast<RE::ActorValueOwner*>(fake)) -
                       reinterpret_cast<std::uint8_t*>(fake);
            }();
            auto* const refr = reinterpret_cast<RE::TESObjectREFR*>(reinterpret_cast<std::uint8_t*>(a_owner) - avOffset);
            return refr->As<RE::Actor>();
        }

        void QueueNpcDefeat(FormID a_victimId, FormID a_aggressorId)
        {
            const auto* const tasks = SFSE::GetTaskInterface();
            if (!tasks) {
                return;
            }
            tasks->AddTask([a_victimId, a_aggressorId]() {
                auto* const victim = a_victimId ? RE::TESForm::LookupByID<RE::Actor>(a_victimId) : nullptr;
                if (!victim || Defeat::IsDefeated(victim)) {
                    return;
                }
                auto* aggressor = a_aggressorId ? RE::TESForm::LookupByID<RE::Actor>(a_aggressorId) : nullptr;
                if (!aggressor) {
                    aggressor = Engine::ResolveCombatTarget(victim);
                }
                if (!Validation::CanProcess() || !Validation::ValidatePair(victim, aggressor)) {
                    return;
                }
                if (Defeat::DefeatActor(victim, aggressor)) {
                    if (auto data = DefeatRegistry::Get(a_victimId)) {
                        data->recoverAtDay = RecoveryDeadlineForSeconds(Config::Get().defaultRecoverySeconds);
                    }
                }
            });
        }

        // Worker-thread classification: auto-defeat hostile NPCs (flesh, or robots when configured).
        [[nodiscard]] bool ShouldAutoDefeatNpc(RE::Actor* a_victim)
        {
            auto* const player = PlayerActor();
            if (!player || !Decision::IsHostile(a_victim, player)) {
                return false;
            }
            return !Decision::IsNonFlesh(a_victim) || Config::NpcAutoDefeatIncludesRobots();
        }

        void ModActorValueDetour(
            RE::ActorValueOwner* a_owner,
            RE::ACTOR_VALUE_MODIFIER a_modifier,
            const RE::ActorValueInfo& a_info,
            float a_delta,
            RE::TESObjectREFR* a_ref)
        {
            if (g_original && a_modifier == RE::ACTOR_VALUE_MODIFIER::kDamage && a_delta < 0.0f && IsLethalGauge(a_info)) {
                if (IsPlayerAVOwner(a_owner)) {
                    RecordAttacker(a_ref);  // participant snapshot source (POD-only on this thread)
                    auto* const player = PlayerActor();
                    const float current = a_owner->GetActorValue(a_info);
                    const bool  lethal = current + a_delta <= 0.0f;
                    auto* const aggressor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
                    if (lethal && Validation::CanProcess() && Validation::ValidatePair(player, aggressor) && !Defeat::IsDefeated(player)) {
                        a_delta = std::max(a_delta, LethalFloorFor(a_owner, a_info) - current);
                        QueueBegin(a_ref);
                    }
                } else if (Config::IsNpcAutoDefeatEnabled()) {
                    auto* const victim = ActorFromAVOwner(a_owner);
                    if (victim && !victim->IsPlayerRef()) {
                        const float current = a_owner->GetActorValue(a_info);
                        const bool  lethal = current + a_delta <= 0.0f;
                        auto* const aggressor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
                        if (lethal && Validation::CanProcess() && Validation::ValidatePair(victim, aggressor) &&
                            ShouldAutoDefeatNpc(victim) && !Defeat::IsDefeated(victim)) {
                            a_delta = std::max(a_delta, LethalFloorFor(a_owner, a_info) - current);
                            QueueNpcDefeat(victim->GetFormID(), a_ref ? a_ref->GetFormID() : 0);
                        }
                    }
                }
            }

            if (g_original) {
                g_original(a_owner, a_modifier, a_info, a_delta, a_ref);
            }
        }

        [[nodiscard]] bool InstallHook()
        {
            const auto target = kModActorValueID.address();
            const auto* const bytes = reinterpret_cast<const std::uint8_t*>(target);
            if (!std::equal(kExpectedPrologue.begin(), kExpectedPrologue.end(), bytes)) {
                REX::ERROR("[OSFDefeat] ModActorValue prologue mismatch; player lethal hook disabled");
                return false;
            }

            auto& trampoline = REL::GetTrampoline();
            auto* const gateway = static_cast<std::uint8_t*>(trampoline.allocate(kStolenLength + sizeof(REL::ASM::JMP14)));
            std::memcpy(gateway, bytes, kStolenLength);
            new (gateway + kStolenLength) REL::ASM::JMP14{ target + kStolenLength };
            g_original = reinterpret_cast<ModActorValue_t*>(gateway);

            const auto branch = trampoline.allocate_branch5(reinterpret_cast<std::uintptr_t>(&ModActorValueDetour));
            const REL::ASM::JMP5 jmp{ target, branch };
            if (!REL::WriteSafeData(target, jmp)) {
                REX::ERROR("[OSFDefeat] failed to patch ModActorValue hook");
                return false;
            }
            if constexpr (kStolenLength > sizeof(jmp)) {
                (void)REL::WriteSafeFill(target + sizeof(jmp), 0x90, kStolenLength - sizeof(jmp));
            }

            REX::INFO("[OSFDefeat] installed player lethal damage hook at {:X}", target);
            return true;
        }

        void StartRecoveryWorker()
        {
            bool expected = false;
            if (!g_workerStarted.compare_exchange_strong(expected, true)) {
                return;
            }

            std::thread([]() {
                for (;;) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    if (const auto* const tasks = SFSE::GetTaskInterface()) {
                        tasks->AddTask([]() { TickRecovery(); });
                    }
                }
            }).detach();
        }
    }

    float RecoveryDeadlineForSeconds(float a_realSeconds)
    {
        return NowDay() + ((std::max(1.0f, a_realSeconds) * TimeScale()) / 86400.0f);
    }

    bool SetRecoveryTimer(RE::Actor* a_actor, float a_seconds)
    {
        if (!a_actor) {
            return false;
        }
        const auto data = DefeatRegistry::Get(a_actor->GetFormID());
        if (!data) {
            return false;
        }
        if (a_seconds > 0.0f) {
            data->recoverAtDay = RecoveryDeadlineForSeconds(a_seconds);
            REX::INFO("[OSFDefeat] recovery timer for {:08X} set to {:.2f}s", a_actor->GetFormID(), a_seconds);
        } else {
            data->recoverAtDay = 0.0f;
            REX::INFO("[OSFDefeat] recovery for {:08X} held; awaiting explicit rescue", a_actor->GetFormID());
        }
        return true;
    }

    float GetRecoverySecondsRemaining(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return 0.0f;
        }
        const auto data = DefeatRegistry::Get(a_actor->GetFormID());
        if (!data) {
            return 0.0f;
        }
        if (data->recoverAtDay <= 0.0f) {
            return -1.0f;
        }
        return std::max(0.0f, (data->recoverAtDay - NowDay()) * 86400.0f / TimeScale());
    }

    void BeginPlayerDefeat(RE::Actor* a_aggressor)
    {
        auto* const player = PlayerActor();
        if (!Validation::CanProcess() || !Validation::ValidatePair(player, a_aggressor) || Defeat::IsDefeated(player)) {
            return;
        }

        const auto outcome = Consequence::SelectPlayerOutcome(player, a_aggressor);
        // Snapshot participants BEFORE the down state applies: DefeatActor drops the player from
        // combatant target lists (and may captive-pacify), which can clear the hostility the
        // participant filter keys on.
        const auto participants = CollectParticipants(player, a_aggressor, Config::ParticipantScanRadius());
        if (!Defeat::DefeatActor(player, a_aggressor)) {
            return;
        }

        if (auto data = DefeatRegistry::Get(player->GetFormID())) {
            // holdRecovery leaves recoverAtDay at 0 (held): the consequence owns the
            // down-state and must end it via RescueActor (or SetRecoveryTimer).
            data->recoverAtDay = outcome.holdRecovery ? 0.0f : RecoveryDeadlineForSeconds(outcome.recoverySeconds);
        }

        if (outcome.holdRecovery) {
            REX::INFO("[OSFDefeat] player outcome '{}' selected; recovery held for content", outcome.name);
        } else {
            REX::INFO("[OSFDefeat] player outcome '{}' selected; recovery in {:.2f}s", outcome.name, outcome.recoverySeconds);
        }

        // Stage the handoff payload (read back through the GetLastPlayer* Papyrus
        // getters) — including the participant snapshot (recent attackers, Acheron
        // member-list parity) — then start the consequence quest, then notify content
        // consumers such as OSF Surrender. Context-before-quest so a quest's
        // OnQuestInit can already read the payload. We are on the game thread here,
        // so the VM dispatches are safe.
        DefeatContext::SetPlayerContext(player, a_aggressor, outcome, participants);
        if (outcome.quest && outcome.startQuest) {
            QuestHandoff::TryStartQuestToken(*outcome.quest);
        }
        Events::BroadcastPlayerConsequenceStarted();
    }

    void TickRecovery()
    {
        // Auto-recover every defeated actor whose deadline has passed. Held entries (recoverAtDay <= 0,
        // e.g. an NPC awaiting an explicit RescueActor from content) are skipped. Snapshot is a copy of
        // IDs, so RescueActor mutating the registry mid-loop is safe.
        const float now = NowDay();
        for (const auto id : DefeatRegistry::Snapshot()) {
            const auto data = DefeatRegistry::Get(id);
            if (!data || data->recoverAtDay <= 0.0f || now < data->recoverAtDay) {
                continue;
            }
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                Defeat::RescueActor(actor, true);
            }
        }
    }

    void Install()
    {
        if (g_hooked.exchange(true)) {
            return;
        }
        g_hooked.store(InstallHook());
        StartRecoveryWorker();
    }
}