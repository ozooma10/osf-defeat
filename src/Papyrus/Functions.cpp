#include "pch.h"

#include "Papyrus/Functions.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IVirtualMachine.h"

#include "Core/Capture.h"
#include "Core/Config.h"
#include "Core/Consequence.h"
#include "Core/Decision.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/Events.h"
#include "Core/PlayerDefeat.h"
#include "Core/Validation.h"
#include "Engine/Engine.h"

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

        // Recovery handshake: content takes ownership of a down-state ("hold, my scene is
        // running") or reschedules it. afSeconds <= 0 holds until an explicit RescueActor.
        bool SetRecoveryTimer(std::monostate, RE::Actor* a_actor, float a_seconds)
        {
            if (!a_actor) {
                return false;
            }

            const FormID actorId = a_actor->GetFormID();
            return QueueGameTask([actorId, a_seconds]() {
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    PlayerDefeat::SetRecoveryTimer(actor, a_seconds);
                }
            });
        }

        float GetRecoverySecondsRemaining(std::monostate, RE::Actor* a_actor)
        {
            return PlayerDefeat::GetRecoverySecondsRemaining(a_actor);
        }

        // Ownership lease: content claims a held down-state under a named owner with a watchdog
        // deadline. Queued to the game thread — check GetClaimOwner afterwards to confirm the
        // claim landed (a competing owner's live lease refuses it).
        bool ClaimDefeat(std::monostate, RE::Actor* a_actor, RE::BSFixedString a_owner, float a_maxHoldSeconds)
        {
            if (!a_actor || !a_owner.c_str() || *a_owner.c_str() == '\0') {
                return false;
            }

            const FormID actorId = a_actor->GetFormID();
            return QueueGameTask([actorId, owner = std::string{ a_owner.c_str() }, a_maxHoldSeconds]() {
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    PlayerDefeat::ClaimDefeat(actor, owner, a_maxHoldSeconds);
                }
            });
        }

        bool ReleaseClaim(std::monostate, RE::Actor* a_actor, RE::BSFixedString a_owner)
        {
            if (!a_actor || !a_owner.c_str()) {
                return false;
            }

            const FormID actorId = a_actor->GetFormID();
            return QueueGameTask([actorId, owner = std::string{ a_owner.c_str() }]() {
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    PlayerDefeat::ReleaseClaim(actor, owner);
                }
            });
        }

        std::string GetClaimOwner(std::monostate, RE::Actor* a_actor)
        {
            return PlayerDefeat::GetClaimOwner(a_actor);
        }

        // Emergency escape / safe uninstall. Console: cgf "OSFDefeat.ForceRecoverAll"
        bool ForceRecoverAll(std::monostate)
        {
            return QueueGameTask([]() {
                Defeat::ForceRecoverAll();
            });
        }

        // How the actor went down: 0 none/unknown, 1 Health, 2 EM, 3 scripted (DefeatActor verb).
        std::int32_t GetDefeatCause(std::monostate, RE::Actor* a_actor)
        {
            return static_cast<std::int32_t>(Defeat::GetDefeatCause(a_actor));
        }

        std::int32_t GetLastPlayerDefeatCause(std::monostate)
        {
            return static_cast<std::int32_t>(DefeatContext::GetLastPlayerDefeatCause());
        }

        // Struggle primitive: per-actor no-defeat grace window (vanilla lethality while active).
        // Thread-safe map writes, so these run inline rather than queued.
        bool ResistDefeat(std::monostate, RE::Actor* a_actor, float a_seconds)
        {
            return PlayerDefeat::SetResistWindow(a_actor, a_seconds);
        }

        bool IsResistingDefeat(std::monostate, RE::Actor* a_actor)
        {
            return PlayerDefeat::IsResistWindowActive(a_actor);
        }

        // Captive-faction pacify on demand (surrender-to-base), independent of the automatic
        // `factionPacify` config gate. Removal also happens automatically on rescue.
        bool ApplyCaptiveFaction(std::monostate, RE::Actor* a_actor)
        {
            if (!a_actor) {
                return false;
            }

            const FormID actorId = a_actor->GetFormID();
            return QueueGameTask([actorId]() {
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    Engine::ApplyCaptiveFaction(actor);
                }
            });
        }

        bool RemoveCaptiveFaction(std::monostate, RE::Actor* a_actor)
        {
            if (!a_actor) {
                return false;
            }

            const FormID actorId = a_actor->GetFormID();
            return QueueGameTask([actorId]() {
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    Engine::RemoveCaptiveFaction(actor);
                }
            });
        }

        bool IsCaptiveFactionApplied(std::monostate, RE::Actor* a_actor)
        {
            return Engine::HasCaptiveFactionApplied(a_actor);
        }

        // Capture hooks (M4): claim a defeated NPC as a captor's prisoner. CaptureActor defeats +
        // holds + pacifies the captive and records the captor; content builds the bounty board on top.
        bool CaptureActor(std::monostate, RE::Actor* a_captive, RE::Actor* a_captor)
        {
            if (!a_captive) {
                return false;
            }
            const FormID captiveId = a_captive->GetFormID();
            const FormID captorId = a_captor ? a_captor->GetFormID() : 0;
            return QueueGameTask([captiveId, captorId]() {
                auto* const captive = RE::TESForm::LookupByID<RE::Actor>(captiveId);
                auto* const captor = captorId ? RE::TESForm::LookupByID<RE::Actor>(captorId) : nullptr;
                if (captive) {
                    Capture::CaptureActor(captive, captor);
                }
            });
        }

        bool ReleaseCaptive(std::monostate, RE::Actor* a_actor, bool a_recover)
        {
            if (!a_actor) {
                return false;
            }
            const FormID actorId = a_actor->GetFormID();
            return QueueGameTask([actorId, a_recover]() {
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    Capture::ReleaseCaptive(actor, a_recover);
                }
            });
        }

        // Move a captive to a marker/ref (brig, ship, cell). Queued so the MoveTo VM-dispatch runs
        // outside the current native frame.
        bool TransferCaptive(std::monostate, RE::Actor* a_actor, RE::TESObjectREFR* a_target)
        {
            if (!a_actor || !a_target) {
                return false;
            }
            const FormID actorId = a_actor->GetFormID();
            const FormID targetId = a_target->GetFormID();
            return QueueGameTask([actorId, targetId]() {
                auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
                auto* const target = RE::TESForm::LookupByID<RE::TESObjectREFR>(targetId);
                if (actor && target) {
                    Capture::TransferCaptive(actor, target);
                }
            });
        }

        bool IsCaptive(std::monostate, RE::Actor* a_actor)
        {
            return Capture::IsCaptive(a_actor);
        }

        RE::Actor* GetCaptor(std::monostate, RE::Actor* a_actor)
        {
            const FormID captorId = Capture::GetCaptorID(a_actor);
            return captorId ? RE::TESForm::LookupByID<RE::Actor>(captorId) : nullptr;
        }

        std::vector<RE::Actor*> GetCaptives(std::monostate, RE::Actor* a_captor)
        {
            return Capture::GetCaptives(a_captor);
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

        std::int32_t RegisterForPlayerDefeatImminent(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, RE::BSFixedString a_callback)
        {
            return Events::RegisterPlayerDefeatImminent(a_receiver, a_callback.c_str());
        }

        bool UnregisterForPlayerDefeatImminent(std::monostate, std::int32_t a_token)
        {
            return Events::UnregisterPlayerDefeatImminent(a_token);
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

        std::vector<RE::Actor*> GetLastPlayerParticipants(std::monostate)
        {
            return DefeatContext::GetLastPlayerParticipants();
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
        vm->BindNativeMethod(kClass, "GetLastPlayerParticipants", &GetLastPlayerParticipants, std::nullopt, false);
        vm->BindNativeMethod(kClass, "SetRecoveryTimer", &SetRecoveryTimer, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetRecoverySecondsRemaining", &GetRecoverySecondsRemaining, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ClaimDefeat", &ClaimDefeat, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ReleaseClaim", &ReleaseClaim, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetClaimOwner", &GetClaimOwner, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ForceRecoverAll", &ForceRecoverAll, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetDefeatCause", &GetDefeatCause, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerDefeatCause", &GetLastPlayerDefeatCause, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ResistDefeat", &ResistDefeat, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsResistingDefeat", &IsResistingDefeat, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RegisterForPlayerDefeatImminent", &RegisterForPlayerDefeatImminent, std::nullopt, false);
        vm->BindNativeMethod(kClass, "UnregisterForPlayerDefeatImminent", &UnregisterForPlayerDefeatImminent, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ApplyCaptiveFaction", &ApplyCaptiveFaction, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RemoveCaptiveFaction", &RemoveCaptiveFaction, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsCaptiveFactionApplied", &IsCaptiveFactionApplied, std::nullopt, false);
        vm->BindNativeMethod(kClass, "CaptureActor", &CaptureActor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ReleaseCaptive", &ReleaseCaptive, std::nullopt, false);
        vm->BindNativeMethod(kClass, "TransferCaptive", &TransferCaptive, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsCaptive", &IsCaptive, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetCaptor", &GetCaptor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetCaptives", &GetCaptives, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ReloadJSON", &ReloadJSON, std::nullopt, false);

        g_bound = true;
        REX::INFO("[OSFDefeat] Papyrus API bound");
    }
}
