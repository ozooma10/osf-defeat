#include "pch.h"

#include "Core/Capture.h"

#include "Core/Defeat.h"
#include "Core/PlayerDefeat.h"
#include "Engine/Engine.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IVirtualMachine.h"

namespace OSFDefeat::Capture
{
    namespace
    {
        struct CaptureRec
        {
            FormID captor{ 0 };
            float  capturedDay{ 0.0f };
        };

        // captiveID -> record. Same map/mutex discipline as Engine's captive-faction tracker:
        // written on the game thread, read from the save thread (Snapshot) under the lock.
        std::map<FormID, CaptureRec> g_captives;
        std::mutex                   g_capturesLock;

        [[nodiscard]] float NowDay()
        {
            const auto* const cal = RE::Calendar::GetSingleton();
            return cal ? cal->GetDaysPassedExact() : 0.0f;
        }
    }

    bool CaptureActor(RE::Actor* a_captive, RE::Actor* a_captor)
    {
        if (!a_captive) {
            return false;
        }

        // Put the captive down if it isn't already; a capture without a down-state is meaningless.
        if (!Defeat::IsDefeated(a_captive)) {
            if (!Defeat::DefeatActor(a_captive, a_captor, DefeatCause::kScripted)) {
                return false;  // dead / un-defeatable
            }
        }

        // Hold it indefinitely (content ends the capture) and pacify it so hostiles ignore the prisoner.
        PlayerDefeat::SetRecoveryTimer(a_captive, 0.0f);
        Engine::ApplyCaptiveFaction(a_captive);

        const FormID captiveId = a_captive->GetFormID();
        const FormID captorId = a_captor ? a_captor->GetFormID() : 0;
        {
            std::unique_lock lock{ g_capturesLock };
            g_captives[captiveId] = CaptureRec{ captorId, NowDay() };
        }
        REX::INFO("[OSFDefeat] captured {:08X} (captor {:08X})", captiveId, captorId);
        return true;
    }

    bool ReleaseCaptive(RE::Actor* a_actor, bool a_recover)
    {
        if (!a_actor) {
            return false;
        }

        bool wasCaptive = false;
        {
            std::unique_lock lock{ g_capturesLock };
            wasCaptive = g_captives.erase(a_actor->GetFormID()) > 0;
        }
        if (!wasCaptive) {
            return false;
        }

        if (a_recover) {
            Defeat::RescueActor(a_actor, true);  // undo-pacify strips the captive faction for us
        } else {
            Engine::RemoveCaptiveFaction(a_actor);  // still held, but no longer a claimed captive
        }
        REX::INFO("[OSFDefeat] released captive {:08X} (recover={})", a_actor->GetFormID(), a_recover);
        return true;
    }

    bool TransferCaptive(RE::Actor* a_actor, RE::TESObjectREFR* a_target)
    {
        if (!a_actor || !a_target || !IsCaptive(a_actor)) {
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
        auto packArgs = [a_target](RE::BSScrapArray<RE::BSScript::Variable>& a_args) {
            a_args.resize(6);
            RE::BSScript::PackVariable(a_args[0], a_target);
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
        std::unique_lock lock{ g_capturesLock };
        return g_captives.contains(a_actor->GetFormID());
    }

    FormID GetCaptorID(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return 0;
        }
        std::unique_lock lock{ g_capturesLock };
        if (const auto it = g_captives.find(a_actor->GetFormID()); it != g_captives.end()) {
            return it->second.captor;
        }
        return 0;
    }

    std::vector<RE::Actor*> GetCaptives(RE::Actor* a_captor)
    {
        const FormID captorFilter = a_captor ? a_captor->GetFormID() : 0;

        std::vector<FormID> ids;
        {
            std::unique_lock lock{ g_capturesLock };
            ids.reserve(g_captives.size());
            for (const auto& [captiveId, rec] : g_captives) {
                if (captorFilter == 0 || rec.captor == captorFilter) {
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
        std::unique_lock lock{ g_capturesLock };
        g_captives.erase(a_captiveId);
    }

    std::vector<std::tuple<FormID, FormID, float>> Snapshot()
    {
        std::unique_lock lock{ g_capturesLock };
        std::vector<std::tuple<FormID, FormID, float>> out;
        out.reserve(g_captives.size());
        for (const auto& [captiveId, rec] : g_captives) {
            out.emplace_back(captiveId, rec.captor, rec.capturedDay);
        }
        return out;
    }

    void RestoreCapture(FormID a_captiveId, FormID a_captorId, float a_capturedDay)
    {
        if (!a_captiveId) {
            return;
        }
        std::unique_lock lock{ g_capturesLock };
        g_captives[a_captiveId] = CaptureRec{ a_captorId, a_capturedDay };
    }

    void Clear()
    {
        std::unique_lock lock{ g_capturesLock };
        g_captives.clear();
    }
}
