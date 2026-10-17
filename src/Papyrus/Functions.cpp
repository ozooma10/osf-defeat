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
#include "Core/Recovery.h"
#include "Core/Truce.h"
#include "Core/RuntimeMutation.h"
#include "Core/Eligibility.h"
#include "Engine/Engine.h"

#include <variant>
#ifdef OSF_DEFEAT_TESTING
#include "AcceptanceTests.h"
#endif

namespace OSFDefeat::Papyrus
{
    namespace
    {
        template <class Fn>
        bool QueueGameTask(Fn a_fn)
        {
            auto* const tasks = RuntimeMutation::GetTaskQueue();
            if (!tasks) {
                return false;
            }
            const auto epoch = RuntimeMutation::CurrentEpoch();
            tasks->AddTask([epoch, fn = std::move(a_fn)]() mutable {
                if (RuntimeMutation::IsCurrent(epoch)) {
                    fn();
                }
            });
            return true;
        }

        bool QueueActorOperation(RE::Actor* a_actor, auto a_operation)
        {
            if (!a_actor) {
                return false;
            }

            const FormID actorId = a_actor->GetFormID();
            return QueueGameTask([actorId, operation = std::move(a_operation)]() {
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    operation(actor);
                }
            });
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
                if (Eligibility::IsEligible(victim)) {
                    Defeat::DefeatActor(victim, aggressor);
                }
            });
        }

        bool RecoverActor(std::monostate, RE::Actor* a_actor, bool a_resumeAI)
        {
            return QueueActorOperation(a_actor, [a_resumeAI](RE::Actor* a_resolvedActor) {
                Defeat::RecoverActor(a_resolvedActor, a_resumeAI);
            });
        }

        bool RecoverActorWithTruce(std::monostate, RE::Actor* a_actor, float a_seconds)
        {
            if (!std::isfinite(a_seconds) || a_seconds <= 0 || a_seconds > 3600) {
                return false;
            }
            return QueueActorOperation(a_actor, [a_seconds](RE::Actor* actor) {
                Defeat::RecoverActor(actor, true, a_seconds);
            });
        }

        bool EndRecoveryTruce(std::monostate, RE::Actor* a_actor)
        {
            return QueueActorOperation(a_actor, [](RE::Actor* actor) { Truce::End(*actor); });
        }

        bool IsRecoveryTruceActive(std::monostate, RE::Actor* a_actor)
        {
            return a_actor && Truce::IsActive(a_actor->GetFormID());
        }

        bool SuspendActorAI(std::monostate, RE::Actor* a_actor)
        {
            return QueueActorOperation(a_actor, &Defeat::SuspendAI);
        }

        bool ResumeActorAI(std::monostate, RE::Actor* a_actor)
        {
            return QueueActorOperation(a_actor, &Defeat::ResumeAI);
        }

        bool ScheduleAutomaticRecovery(std::monostate, RE::Actor* a_actor, float a_delaySeconds)
        {
            return QueueActorOperation(a_actor, [a_delaySeconds](RE::Actor* a_resolvedActor) {
                Recovery::ScheduleAutomatic(a_resolvedActor, a_delaySeconds);
            });
        }

        bool HoldRecovery(std::monostate, RE::Actor* a_actor)
        {
            return QueueActorOperation(a_actor, &Recovery::Hold);
        }

        float GetAutomaticRecoverySecondsRemaining(std::monostate, RE::Actor* a_actor)
        {
            return Recovery::SecondsRemaining(a_actor);
        }

        // Compatibility verb: positive seconds schedule automatic recovery; non-positive holds it.
        bool SetRecoveryTimer(std::monostate, RE::Actor* a_actor, float a_seconds)
        {
            return QueueActorOperation(a_actor, [a_seconds](RE::Actor* a_resolvedActor) {
                Recovery::SetTimer(a_resolvedActor, a_seconds);
            });
        }

        // Defeat lease: a named, watchdog-protected recovery hold.
        bool AcquireDefeatLease(std::monostate, RE::Actor* a_actor, RE::BSFixedString a_holderId, float a_timeoutSeconds)
        {
            if (a_holderId.empty()) {
                return false;
            }

            return QueueActorOperation(a_actor, [holderId = std::string{ a_holderId.c_str() }, a_timeoutSeconds](RE::Actor* a_resolvedActor) {
                Recovery::AcquireLease(a_resolvedActor, holderId, a_timeoutSeconds);
            });
        }

        bool ReleaseDefeatLease(std::monostate, RE::Actor* a_actor, RE::BSFixedString a_holderId)
        {
            return QueueActorOperation(a_actor, [holderId = std::string{ a_holderId.c_str() }](RE::Actor* a_resolvedActor) {
                Recovery::ReleaseLease(a_resolvedActor, holderId);
            });
        }

        std::string GetDefeatLeaseHolder(std::monostate, RE::Actor* a_actor)
        {
            return Recovery::GetLeaseHolder(a_actor);
        }

        // Emergency escape / safe uninstall. Console: cgf "OSFDefeat.ForceRecoverAll"
        bool ForceRecoverAll(std::monostate)
        {
            return QueueGameTask([]() {
                Defeat::ForceRecoverAll();
            });
        }

        // How the actor went down: 0 none/unknown, 1 Health, 2 scripted (DefeatActor verb).
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

        // Captive-faction non-hostility on demand (surrender-to-base), independent of the automatic
        // player-defeat setting. Removal also happens automatically on recovery.
        bool ApplyCaptiveFaction(std::monostate, RE::Actor* a_actor)
        {
            return QueueActorOperation(a_actor, [](RE::Actor* a_resolvedActor) {
                Engine::ApplyCaptiveFaction(*a_resolvedActor);
            });
        }

        bool RemoveCaptiveFaction(std::monostate, RE::Actor* a_actor)
        {
            return QueueActorOperation(a_actor, [](RE::Actor* a_resolvedActor) {
                Engine::RemoveCaptiveFaction(*a_resolvedActor);
            });
        }

        bool IsCaptiveFactionApplied(std::monostate, RE::Actor* a_actor)
        {
            return a_actor && Engine::HasCaptiveFactionApplied(*a_actor);
        }

        // Capture hooks (M4): register a defeated NPC as a captor's prisoner. CaptureActor defeats,
        // holds, suspends AI, applies non-hostility, and records the captor.
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
            return QueueActorOperation(a_actor, [a_recover](RE::Actor* a_resolvedActor) {
                Capture::ReleaseCaptive(a_resolvedActor, a_recover);
            });
        }

        // Move a captive to a marker/ref (brig, ship, cell). Dispatch the MoveTo VM request
        // through the engine queue; it may run inline when already on the drain owner.
        bool TransferCaptive(std::monostate, RE::Actor* a_actor, RE::TESObjectREFR* a_target)
        {
            if (!a_actor || !a_target) {
                return false;
            }
            const FormID actorId = a_actor->GetFormID();
            const FormID targetId = a_target->GetFormID();
            return QueueGameTask([actorId, targetId]() {
                auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
                // The player ref (0x14) is NOT retrievable via LookupByID in this build — it's a
                // singleton, not in the form table by ID. Resolve it directly; fall back to the
                // form table for any other target (marker, ship, cell ref).
                auto* const player = RE::PlayerCharacter::GetSingleton();
                auto* const target = (player && player->GetFormID() == targetId)
                    ? static_cast<RE::TESObjectREFR*>(player)
                    : RE::TESForm::LookupByID<RE::TESObjectREFR>(targetId);
                if (actor && target) {
                    Capture::TransferCaptive(actor, target);
                } else {
                    REX::WARN("[OSFDefeat] transfer captive: deferred lookup failed (actor {:08X}={}, target {:08X}={})",
                        actorId, actor ? "ok" : "null", targetId, target ? "ok" : "null");
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

        bool IsActorAISuspended(std::monostate, RE::Actor* a_actor) { return Defeat::IsAISuspended(a_actor); }

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
            Eligibility::SetEnabled(!a_disable);
        }

        bool IsProcessingDisabled(std::monostate)
        {
            return !Eligibility::IsEnabled();
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

        std::string GetLastPlayerConsequenceId(std::monostate)
        {
            return DefeatContext::GetLastPlayerConsequenceId();
        }

        std::string GetLastPlayerConsequenceQuest(std::monostate)
        {
            return DefeatContext::GetLastPlayerConsequenceQuest();
        }

        float GetLastPlayerAutoRecoveryDelaySeconds(std::monostate)
        {
            return DefeatContext::GetLastPlayerAutoRecoveryDelaySeconds();
        }

        bool IsLastPlayerRecoveryHeld(std::monostate)
        {
            return DefeatContext::IsLastPlayerRecoveryHeld();
        }

        std::vector<RE::Actor*> GetLastPlayerRecentHostileAttackers(std::monostate)
        {
            return DefeatContext::GetLastPlayerRecentHostileAttackers();
        }

        void ReloadJSON(std::monostate)
        {
            Config::Reload();
            Consequence::Reload();
        }
    }

    void Register()
    {
        const auto* const gameVM = RE::GameVM::GetSingleton();
        auto* const vm = gameVM ? gameVM->GetVM() : nullptr;
        if (!vm) {
            REX::WARN("[OSFDefeat] Papyrus VM not ready - API not bound");
            return;
        }

        constexpr std::string_view kClass = "OSFDefeat";
        vm->BindNativeMethod(kClass, "DefeatActor", &DefeatActor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RecoverActor", &RecoverActor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RecoverActorWithTruce", &RecoverActorWithTruce, std::nullopt, false);
        vm->BindNativeMethod(kClass, "EndRecoveryTruce", &EndRecoveryTruce, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsRecoveryTruceActive", &IsRecoveryTruceActive, std::nullopt, false);
        vm->BindNativeMethod(kClass, "RescueActor", &RecoverActor, std::nullopt, false);
        vm->BindNativeMethod(kClass, "SuspendActorAI", &SuspendActorAI, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ResumeActorAI", &ResumeActorAI, std::nullopt, false);
        vm->BindNativeMethod(kClass, "PacifyActor", &SuspendActorAI, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ReleaseActor", &ResumeActorAI, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsDefeated", &IsDefeated, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsPacified", &IsActorAISuspended, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsActorAISuspended", &IsActorAISuspended, std::nullopt, false);
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
        vm->BindNativeMethod(kClass, "GetLastPlayerConsequenceId", &GetLastPlayerConsequenceId, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerConsequenceName", &GetLastPlayerConsequenceName, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerConsequenceQuest", &GetLastPlayerConsequenceQuest, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerRecoverySeconds", &GetLastPlayerAutoRecoveryDelaySeconds, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerAutoRecoveryDelaySeconds", &GetLastPlayerAutoRecoveryDelaySeconds, std::nullopt, false);
        vm->BindNativeMethod(kClass, "IsLastPlayerRecoveryHeld", &IsLastPlayerRecoveryHeld, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerParticipants", &GetLastPlayerRecentHostileAttackers, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetLastPlayerRecentHostileAttackers", &GetLastPlayerRecentHostileAttackers, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ScheduleAutomaticRecovery", &ScheduleAutomaticRecovery, std::nullopt, false);
        vm->BindNativeMethod(kClass, "HoldRecovery", &HoldRecovery, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetAutomaticRecoverySecondsRemaining", &GetAutomaticRecoverySecondsRemaining, std::nullopt, false);
        vm->BindNativeMethod(kClass, "SetRecoveryTimer", &SetRecoveryTimer, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetRecoverySecondsRemaining", &GetAutomaticRecoverySecondsRemaining, std::nullopt, false);
        vm->BindNativeMethod(kClass, "AcquireDefeatLease", &AcquireDefeatLease, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ReleaseDefeatLease", &ReleaseDefeatLease, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetDefeatLeaseHolder", &GetDefeatLeaseHolder, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ClaimDefeat", &AcquireDefeatLease, std::nullopt, false);
        vm->BindNativeMethod(kClass, "ReleaseClaim", &ReleaseDefeatLease, std::nullopt, false);
        vm->BindNativeMethod(kClass, "GetClaimOwner", &GetDefeatLeaseHolder, std::nullopt, false);
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

#ifdef OSF_DEFEAT_TESTING
        RuntimeTests::Acceptance::Bind(*vm);
#endif
        REX::INFO("[OSFDefeat] Papyrus API bound");
    }
}
