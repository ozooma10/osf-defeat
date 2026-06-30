#include "pch.h"

#include "Papyrus/Functions.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IVirtualMachine.h"

#include "Core/Config.h"
#include "Core/Consequence.h"
#include "Core/Decision.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/Events.h"
#include "Core/Validation.h"

#include <variant>

namespace OSFDefeat::Papyrus
{
    namespace
    {
        template <class Fn>
        bool QueueGameTask(Fn a_fn)
        {
            const auto* const tasks = SFSE::GetTaskInterface();
            if (!tasks) {
                return false;
            }
            tasks->AddTask(std::move(a_fn));
            return true;
        }

        bool DefeatActor(std::monostate, RE::Actor* a_victim, RE::Actor* a_aggressor)
        {
            if (!a_victim) {
                return false;
            }

            const FormID victimId = a_victim->GetFormID();
            const FormID aggressorId = a_aggressor ? a_aggressor->GetFormID() : 0;
            return QueueGameTask([victimId, aggressorId]() {
                auto* const victim = RE::TESForm::LookupByID<RE::Actor>(victimId);
                auto* const aggressor = aggressorId ? RE::TESForm::LookupByID<RE::Actor>(aggressorId) : nullptr;
                if (Validation::CanProcess() && Validation::ValidatePair(victim, aggressor)) {
                    Defeat::DefeatActor(victim, aggressor);
                }
            });
        }

        bool RescueActor(std::monostate, RE::Actor* a_victim, bool a_undoPacify)
        {
            if (!a_victim) {
                return false;
            }

            const FormID victimId = a_victim->GetFormID();
            return QueueGameTask([victimId, a_undoPacify]() {
                if (auto* const victim = RE::TESForm::LookupByID<RE::Actor>(victimId)) {
                    Defeat::RescueActor(victim, a_undoPacify);
                }
            });
        }

        bool PacifyActor(std::monostate, RE::Actor* a_actor)
        {
            if (!a_actor) {
                return false;
            }

            const FormID actorId = a_actor->GetFormID();
            return QueueGameTask([actorId]() {
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    Defeat::Pacify(actor);
                }
            });
        }

        bool ReleaseActor(std::monostate, RE::Actor* a_actor)
        {
            if (!a_actor) {
                return false;
            }

            const FormID actorId = a_actor->GetFormID();
            return QueueGameTask([actorId]() {
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    Defeat::UndoPacify(actor);
                }
            });
        }

        bool IsDefeated(std::monostate, RE::Actor* a_actor)
        {
            return Defeat::IsDefeated(a_actor);
        }

        bool IsPacified(std::monostate, RE::Actor* a_actor)
        {
            return Defeat::IsPacified(a_actor);
        }

        // Classification for content NPC-consequence routing (and the future NPC auto-defeat gate):
        // 0 = flesh, 1 = robot, 2 = turret.
        std::int32_t ClassifyActor(std::monostate, RE::Actor* a_actor)
        {
            return static_cast<std::int32_t>(Decision::Classify(a_actor));
        }

        bool IsNonFlesh(std::monostate, RE::Actor* a_actor)
        {
            return Decision::IsNonFlesh(a_actor);
        }

        bool IsHostile(std::monostate, RE::Actor* a_actor, RE::Actor* a_target)
        {
            return Decision::IsHostile(a_actor, a_target);
        }

        std::vector<RE::Actor*> GetDefeated(std::monostate, bool a_loadedOnly)
        {
            return Defeat::GetAllDefeated(a_loadedOnly);
        }

        void DisableProcessing(std::monostate, bool a_disable)
        {
            Validation::SetEnabled(!a_disable);
        }

        bool IsProcessingDisabled(std::monostate)
        {
            return !Validation::IsEnabled();
        }

        // Event delivery is callback-style (Starfield can't deliver Papyrus events to non-native
        // scripts): a consumer registers its script + a function name and gets a token back.
        std::int32_t RegisterForPlayerDefeated(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, RE::BSFixedString a_callback)
        {
            return Events::RegisterPlayerDefeated(a_receiver, a_callback.c_str());
        }

        bool UnregisterForPlayerDefeated(std::monostate, std::int32_t a_token)
        {
            return Events::UnregisterPlayerDefeated(a_token);
        }

        std::int32_t RegisterForPlayerRecovered(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, RE::BSFixedString a_callback)
        {
            return Events::RegisterPlayerRecovered(a_receiver, a_callback.c_str());
        }

        bool UnregisterForPlayerRecovered(std::monostate, std::int32_t a_token)
        {
            return Events::UnregisterPlayerRecovered(a_token);
        }

        std::int32_t RegisterForPlayerConsequenceStarted(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, RE::BSFixedString a_callback)
        {
            return Events::RegisterPlayerConsequenceStarted(a_receiver, a_callback.c_str());
        }

        bool UnregisterForPlayerConsequenceStarted(std::monostate, std::int32_t a_token)
        {
            return Events::UnregisterPlayerConsequenceStarted(a_token);
        }

        std::int32_t RegisterForActorDefeated(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, RE::BSFixedString a_callback)
        {
            return Events::RegisterActorDefeated(a_receiver, a_callback.c_str());
        }

        bool UnregisterForActorDefeated(std::monostate, std::int32_t a_token)
        {
            return Events::UnregisterActorDefeated(a_token);
        }

        std::int32_t RegisterForActorRecovered(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, RE::BSFixedString a_callback)
        {
            return Events::RegisterActorRecovered(a_receiver, a_callback.c_str());
        }

        bool UnregisterForActorRecovered(std::monostate, std::int32_t a_token)
        {
            return Events::UnregisterActorRecovered(a_token);
        }

        RE::Actor* GetLastPlayerAggressor(std::monostate)
        {
            return DefeatContext::GetLastPlayerAggressor();
        }

        std::string GetLastPlayerConsequenceName(std::monostate)
        {
            return DefeatContext::GetLastPlayerConsequenceName();
        }

        std::string GetLastPlayerConsequenceQuest(std::monostate)
        {
            return DefeatContext::GetLastPlayerConsequenceQuest();
        }

        float GetLastPlayerRecoverySeconds(std::monostate)
        {
            return DefeatContext::GetLastPlayerRecoverySeconds();
        }

        void ReloadJSON(std::monostate)
        {
            Config::Reload();
            Consequence::Reload();
        }

        bool g_bound = false;
    }

    void Register()
    {
        if (g_bound) {
            return;
        }

        const auto* const gameVM = RE::GameVM::GetSingleton();
        auto* const vm = gameVM ? gameVM->GetVM() : nullptr;
        if (!vm) {
            REX::WARN("[OSFDefeat] Papyrus VM not ready - API not bound");
            return;
        }

        constexpr std::string_view kClass = "OSFDefeat";
        vm->BindNativeMethod(kClass, "DefeatActor", &DefeatActor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RescueActor", &RescueActor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "PacifyActor", &PacifyActor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ReleaseActor", &ReleaseActor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsDefeated", &IsDefeated, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsPacified", &IsPacified, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ClassifyActor", &ClassifyActor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsNonFlesh", &IsNonFlesh, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsHostile", &IsHostile, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetDefeated", &GetDefeated, std::nullopt, false);
        vm->BindNativeMethod(kClass, "DisableProcessing", &DisableProcessing, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsProcessingDisabled", &IsProcessingDisabled, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RegisterForPlayerDefeated", &RegisterForPlayerDefeated, std::nullopt, false);
        vm->BindNativeMethod(kClass, "UnregisterForPlayerDefeated", &UnregisterForPlayerDefeated, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RegisterForPlayerRecovered", &RegisterForPlayerRecovered, std::nullopt, false);
        vm->BindNativeMethod(kClass, "UnregisterForPlayerRecovered", &UnregisterForPlayerRecovered, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RegisterForPlayerConsequenceStarted", &RegisterForPlayerConsequenceStarted, std::nullopt, false);
        vm->BindNativeMethod(kClass, "UnregisterForPlayerConsequenceStarted", &UnregisterForPlayerConsequenceStarted, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RegisterForActorDefeated", &RegisterForActorDefeated, std::nullopt, false);
        vm->BindNativeMethod(kClass, "UnregisterForActorDefeated", &UnregisterForActorDefeated, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RegisterForActorRecovered", &RegisterForActorRecovered, std::nullopt, false);
        vm->BindNativeMethod(kClass, "UnregisterForActorRecovered", &UnregisterForActorRecovered, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerAggressor", &GetLastPlayerAggressor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerConsequenceName", &GetLastPlayerConsequenceName, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerConsequenceQuest", &GetLastPlayerConsequenceQuest, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerRecoverySeconds", &GetLastPlayerRecoverySeconds, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ReloadJSON", &ReloadJSON, std::nullopt, false);

        g_bound = true;
        REX::INFO("[OSFDefeat] Papyrus API bound");
    }
}
