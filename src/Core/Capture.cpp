#include "pch.h"

#include "Core/Capture.h"

#include "Core/Defeat.h"
#include "Core/Recovery.h"
#include "Core/RuntimeMutation.h"
#include "Engine/Engine.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IVirtualMachine.h"

namespace OSFDefeat::Capture
{
    namespace
    {
        struct CaptureRelationshipData
        {
            FormID captor{ 0 };
            float  capturedDay{ 0.0f };
            bool   removeOwnedFactionOnRelease{ false };
        };

        // captiveID -> record. Same map/mutex discipline as Engine's captive-faction tracker:
        // written on the game thread, read from the save thread (Snapshot) under the lock.
        std::map<FormID, CaptureRelationshipData> g_captureRelationships;
        std::mutex g_captureRelationshipsLock;

        [[nodiscard]] float NowDay()
        {
            const auto* const cal = RE::Calendar::GetSingleton();
            return cal ? cal->GetDaysPassedExact() : 0.0f;
        }
    }

    bool CaptureActor(RE::Actor* a_captive, RE::Actor* a_captor)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_captive || !Engine::IsCaptiveFactionAvailable()) {
            if (a_captive) {
                REX::WARN("[OSFDefeat] capture {:08X} refused: captive faction unavailable", a_captive->GetFormID());
            }
            return false;
        }

        // Put the captive down if it isn't already; a capture without a down-state is meaningless.
        const auto existingData = DefeatRegistry::Get(a_captive->GetFormID());
        const bool newlyDefeated = !existingData;
        float previousAutoRecoverAtDay = 0.0f;
        float previousLeaseExpiresAtDay = 0.0f;
        std::string previousLeaseHolderId;
        if (existingData) {
            previousAutoRecoverAtDay = existingData->autoRecoverAtDay.load(std::memory_order_relaxed);
            previousLeaseExpiresAtDay = existingData->leaseExpiresAtDay.load(std::memory_order_relaxed);
            previousLeaseHolderId = existingData->CopyLeaseHolderId();
        } else if (!Defeat::DefeatActor(a_captive, a_captor, DefeatCause::kScripted)) {
            return false;  // dead / un-defeatable
        }

        // Hold it indefinitely (content ends the capture), suspend its AI, and apply non-hostility.
        const bool hadOwnedFaction = Engine::HasCaptiveFactionApplied(*a_captive);
        const bool recoveryHeld = Recovery::Hold(a_captive);
        Engine::ApplyCaptiveFaction(*a_captive);
        const bool factionReady = Engine::HasCaptiveFactionApplied(*a_captive) || Engine::IsInCaptiveFaction(*a_captive);
        if (!recoveryHeld || !factionReady) {
            REX::ERROR("[OSFDefeat] capture {:08X} failed to establish {}{}; rolling back",
                a_captive->GetFormID(), recoveryHeld ? "" : "recovery hold", !factionReady ? " captive faction" : "");
            if (!hadOwnedFaction) {
                Engine::RemoveCaptiveFaction(*a_captive);
            }
            if (newlyDefeated) {
                Defeat::RecoverActor(a_captive, true);
            } else {
                existingData->autoRecoverAtDay = previousAutoRecoverAtDay;
                existingData->leaseExpiresAtDay = previousLeaseExpiresAtDay;
                existingData->SetLeaseHolderId(std::move(previousLeaseHolderId));
            }
            return false;
        }

        const FormID captiveId = a_captive->GetFormID();
        const FormID captorId = a_captor ? a_captor->GetFormID() : 0;
        {
            std::unique_lock lock{ g_captureRelationshipsLock };
            const bool previouslyOwned = g_captureRelationships.contains(captiveId) && g_captureRelationships[captiveId].removeOwnedFactionOnRelease;
            const bool captureAddedFaction = !hadOwnedFaction && Engine::HasCaptiveFactionApplied(*a_captive);
            g_captureRelationships[captiveId] = CaptureRelationshipData{ captorId, NowDay(), previouslyOwned || captureAddedFaction };
        }
        REX::INFO("[OSFDefeat] captured {:08X} (captor {:08X})", captiveId, captorId);
        return true;
    }

    bool ReleaseCaptive(RE::Actor* a_actor, bool a_recover)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_actor) {
            return false;
        }

        std::optional<bool> removeOwnedFactionOnRelease;
        {
            std::unique_lock lock{ g_captureRelationshipsLock };
            if (const auto it = g_captureRelationships.find(a_actor->GetFormID()); it != g_captureRelationships.end()) {
                removeOwnedFactionOnRelease = it->second.removeOwnedFactionOnRelease;
            }
        }
        if (!removeOwnedFactionOnRelease.has_value()) {
            return false;
        }

        if (a_recover) {
            // Recovery owns removal through NotifyRecovered. If recovery cannot commit, preserve the
            // capture relationship so callers can retry instead of reporting a partial release.
            if (!Defeat::RecoverActor(a_actor, true)) {
                return false;
            }
        } else {
            NotifyRecovered(a_actor->GetFormID());
            if (*removeOwnedFactionOnRelease) {
                Engine::RemoveCaptiveFaction(*a_actor);  // remove only membership capture introduced
            }
        }
        REX::INFO("[OSFDefeat] released captive {:08X} (recover={})", a_actor->GetFormID(), a_recover);
        return true;
    }

    bool TransferCaptive(RE::Actor* a_actor, RE::TESObjectREFR* a_target)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_actor || !a_target) {
            REX::WARN("[OSFDefeat] transfer captive: null {}", a_actor ? "target" : "actor");
            return false;
        }
        if (!IsCaptive(a_actor)) {
            REX::WARN("[OSFDefeat] transfer captive {:08X}: no capture relationship (CaptureActor first)",
                a_actor->GetFormID());
            return false;
        }

        const auto* const gameVM = RE::GameVM::GetSingleton();
        auto* const vm = gameVM ? gameVM->GetVM() : nullptr;
        if (!vm) {
            REX::WARN("[OSFDefeat] Papyrus VM unavailable; cannot transfer captive {:08X}", a_actor->GetFormID());
            return false;
        }

        const auto& handles = vm->GetObjectHandlePolicy();
        const auto handle = handles.GetHandleForObject(
            RE::BSScript::GetVMTypeID<RE::Actor>(),
            static_cast<const void*>(a_actor));
        if (handle == handles.EmptyHandle()) {
            REX::WARN("[OSFDefeat] no VM handle for captive {:08X}", a_actor->GetFormID());
            return false;
        }

        // ObjectReference.MoveTo(akTarget, afXOffset, afYOffset, afZOffset, abMatchRotation, abRotateOffset).
        // Six params (the trailing two default in Papyrus, but the VM dispatch fills the whole stack):
        // match the marker's rotation, no rotated offset.
        const FormID targetId = a_target->GetFormID();
        auto packArgs = [targetId](RE::BSScrapArray<RE::BSScript::Variable>& a_args) {
            auto* const player = RE::PlayerCharacter::GetSingleton();
            auto* const target = player && player->GetFormID() == targetId
                ? static_cast<RE::TESObjectREFR*>(player)
                : RE::TESForm::LookupByID<RE::TESObjectREFR>(targetId);
            if (!target) {
                REX::WARN("[OSFDefeat] transfer captive: target {:08X} unloaded before VM argument packing", targetId);
                return false;
            }
            a_args.resize(6);
            RE::BSScript::PackVariable(a_args[0], target);
            RE::BSScript::PackVariable(a_args[1], 0.0f);
            RE::BSScript::PackVariable(a_args[2], 0.0f);
            RE::BSScript::PackVariable(a_args[3], 0.0f);
            RE::BSScript::PackVariable(a_args[4], true);
            RE::BSScript::PackVariable(a_args[5], false);
            return true;
        };
        const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> noCallback{};
        const bool ok = vm->DispatchMethodCall(handle, "ObjectReference", "MoveTo", packArgs, noCallback, 0);
        REX::INFO("[OSFDefeat] transfer captive {:08X} -> {:08X}: dispatch {}",
            a_actor->GetFormID(), a_target->GetFormID(), ok ? "ok" : "failed");
        return ok;
    }

    bool IsCaptive(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }
        std::unique_lock lock{ g_captureRelationshipsLock };
        return g_captureRelationships.contains(a_actor->GetFormID());
    }

    FormID GetCaptorID(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return 0;
        }
        std::unique_lock lock{ g_captureRelationshipsLock };
        if (const auto it = g_captureRelationships.find(a_actor->GetFormID()); it != g_captureRelationships.end()) {
            return it->second.captor;
        }
        return 0;
    }

    std::vector<RE::Actor*> GetCaptives(RE::Actor* a_captor)
    {
        const FormID captorFilter = a_captor ? a_captor->GetFormID() : 0;

        std::vector<FormID> ids;
        {
            std::unique_lock lock{ g_captureRelationshipsLock };
            ids.reserve(g_captureRelationships.size());
            for (const auto& [captiveId, relationship] : g_captureRelationships) {
                if (captorFilter == 0 || relationship.captor == captorFilter) {
                    ids.push_back(captiveId);
                }
            }
        }

        std::vector<RE::Actor*> out;
        out.reserve(ids.size());
        for (const auto id : ids) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                out.push_back(actor);
            }
        }
        return out;
    }

    void NotifyRecovered(FormID a_captiveId)
    {
        std::unique_lock lock{ g_captureRelationshipsLock };
        g_captureRelationships.erase(a_captiveId);
    }

    std::vector<CaptureRelationshipRecord> Snapshot()
    {
        std::unique_lock lock{ g_captureRelationshipsLock };
        std::vector<CaptureRelationshipRecord> out;
        out.reserve(g_captureRelationships.size());
        for (const auto& [captiveId, relationship] : g_captureRelationships) {
            out.push_back({ captiveId, relationship.captor, relationship.capturedDay,
                relationship.removeOwnedFactionOnRelease });
        }
        return out;
    }

    void RestoreCapture(FormID a_captiveId, FormID a_captorId, float a_capturedDay, bool a_removeOwnedFactionOnRelease)
    {
        if (!a_captiveId) {
            return;
        }
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        std::unique_lock lock{ g_captureRelationshipsLock };
        g_captureRelationships[a_captiveId] = CaptureRelationshipData{ a_captorId, a_capturedDay, a_removeOwnedFactionOnRelease };
    }

    void Clear()
    {
        std::unique_lock lock{ g_captureRelationshipsLock };
        g_captureRelationships.clear();
    }
}
