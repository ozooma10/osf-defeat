#include "pch.h"

#include "Core/PlayerDefeat.h"

#include "Core/Consequence.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/Events.h"
#include "Core/DefeatRegistry.h"
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

        void ModActorValueDetour(
            RE::ActorValueOwner* a_owner,
            RE::ACTOR_VALUE_MODIFIER a_modifier,
            const RE::ActorValueInfo& a_info,
            float a_delta,
            RE::TESObjectREFR* a_ref)
        {
            if (g_original && a_modifier == RE::ACTOR_VALUE_MODIFIER::kDamage && a_delta < 0.0f &&
                IsPlayerAVOwner(a_owner) && IsLethalGauge(a_info)) {
                auto* const player = PlayerActor();
                const float current = a_owner->GetActorValue(a_info);
                const bool lethal = current + a_delta <= 0.0f;
                auto* const aggressor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
                if (lethal && Validation::CanProcess() && Validation::ValidatePair(player, aggressor) && !Defeat::IsDefeated(player)) {
                    a_delta = std::max(a_delta, LethalFloorFor(a_owner, a_info) - current);
                    QueueBegin(a_ref);
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

    void BeginPlayerDefeat(RE::Actor* a_aggressor)
    {
        auto* const player = PlayerActor();
        if (!Validation::CanProcess() || !Validation::ValidatePair(player, a_aggressor) || Defeat::IsDefeated(player)) {
            return;
        }

        const auto outcome = Consequence::SelectPlayerOutcome();
        if (!Defeat::DefeatActor(player, a_aggressor)) {
            return;
        }

        if (auto data = DefeatRegistry::Get(player->GetFormID())) {
            data->recoverAtDay = RecoveryDeadlineForSeconds(outcome.recoverySeconds);
        }

        REX::INFO("[OSFDefeat] player outcome '{}' selected; recovery in {:.2f}s", outcome.name, outcome.recoverySeconds);
        if (outcome.quest) {
            REX::INFO("[OSFDefeat] outcome '{}' carries quest token '{}'; handing off to consequence consumers", outcome.name, *outcome.quest);
        }

        // Stage the handoff payload (read back through the GetLastPlayer* Papyrus
        // getters), then notify content consumers such as OSF Surrender. We are on
        // the game thread here, so the VM SendEvent inside Broadcast is safe.
        DefeatContext::SetPlayerContext(player, a_aggressor, outcome);
        Events::BroadcastPlayerConsequenceStarted();
    }

    void TickRecovery()
    {
        auto* const player = PlayerActor();
        if (!player || !Defeat::IsDefeated(player)) {
            return;
        }

        const auto data = DefeatRegistry::Get(player->GetFormID());
        if (!data || data->recoverAtDay <= 0.0f || NowDay() < data->recoverAtDay) {
            return;
        }

        Defeat::RescueActor(player, true);
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