#include "pch.h"

#include "Core/Events.h"

#include "Core/Defeat.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IObjectHandlePolicy.h"
#include "RE/I/IVirtualMachine.h"

namespace OSFDefeat::Events
{
    namespace
    {
        std::set<FormID> g_playerDefeated;
        std::set<FormID> g_playerRecovered;
        std::set<FormID> g_playerConsequenceStarted;
        std::set<FormID> g_actorDefeated;
        std::set<FormID> g_actorRecovered;
        std::shared_mutex g_lock;

        [[nodiscard]] RE::BSScript::IVirtualMachine* VM()
        {
            const auto* const gameVM = RE::GameVM::GetSingleton();
            return gameVM ? gameVM->GetVM() : nullptr;
        }

        void Send(RE::TESForm* a_receiver, const RE::BSFixedString& a_event)
        {
            auto* const vm = VM();
            if (!vm || !a_receiver) {
                return;
            }

            auto& handles = vm->GetObjectHandlePolicy();
            const auto handle = handles.GetHandleForObject(static_cast<std::uint32_t>(a_receiver->GetFormType()), a_receiver);
            if (handle == handles.EmptyHandle()) {
                REX::WARN("[OSFDefeat] no VM handle for event receiver {:08X}", a_receiver->GetFormID());
                return;
            }

            vm->SendEvent(
                handle,
                a_event,
                [](RE::BSScrapArray<RE::BSScript::Variable>&) { return true; },
                [](const RE::BSTSmartPointer<RE::BSScript::Object>&) { return true; },
                nullptr,
                0);
        }

        void Broadcast(const std::set<FormID>& a_receivers, std::string_view a_event)
        {
            std::vector<FormID> receivers;
            {
                std::shared_lock lock{ g_lock };
                receivers.assign(a_receivers.begin(), a_receivers.end());
            }

            const RE::BSFixedString eventName{ a_event };
            for (const auto id : receivers) {
                if (auto* const receiver = RE::TESForm::LookupByID(id)) {
                    Send(receiver, eventName);
                }
            }
            REX::INFO("[OSFDefeat] broadcast {} to {} receiver(s)", a_event, receivers.size());
        }

        // Like Send, but carries (Actor akVictim, Actor akAggressor) event args.
        void SendActor(RE::TESForm* a_receiver, const RE::BSFixedString& a_event, RE::Actor* a_victim, RE::Actor* a_aggressor)
        {
            auto* const vm = VM();
            if (!vm || !a_receiver) {
                return;
            }

            auto&      handles = vm->GetObjectHandlePolicy();
            const auto handle = handles.GetHandleForObject(static_cast<std::uint32_t>(a_receiver->GetFormType()), a_receiver);
            if (handle == handles.EmptyHandle()) {
                REX::WARN("[OSFDefeat] no VM handle for event receiver {:08X}", a_receiver->GetFormID());
                return;
            }

            vm->SendEvent(
                handle,
                a_event,
                [a_victim, a_aggressor](RE::BSScrapArray<RE::BSScript::Variable>& a_args) {
                    a_args.resize(2);
                    RE::BSScript::PackVariable(a_args[0], a_victim);
                    RE::BSScript::PackVariable(a_args[1], a_aggressor);
                    return true;
                },
                [](const RE::BSTSmartPointer<RE::BSScript::Object>&) { return true; },
                nullptr,
                0);
        }

        void BroadcastActors(const std::set<FormID>& a_receivers, std::string_view a_event, FormID a_victim, FormID a_aggressor)
        {
            std::vector<FormID> receivers;
            {
                std::shared_lock lock{ g_lock };
                receivers.assign(a_receivers.begin(), a_receivers.end());
            }
            if (receivers.empty()) {
                return;
            }

            auto* const victim = a_victim ? RE::TESForm::LookupByID<RE::Actor>(a_victim) : nullptr;
            auto* const aggressor = a_aggressor ? RE::TESForm::LookupByID<RE::Actor>(a_aggressor) : nullptr;
            const RE::BSFixedString eventName{ a_event };
            for (const auto id : receivers) {
                if (auto* const receiver = RE::TESForm::LookupByID(id)) {
                    SendActor(receiver, eventName, victim, aggressor);
                }
            }
            REX::INFO("[OSFDefeat] broadcast {} (victim {:08X}) to {} receiver(s)", a_event, a_victim, receivers.size());
        }

        void Add(std::set<FormID>& a_set, RE::TESForm* a_receiver)
        {
            if (!a_receiver) {
                return;
            }
            std::unique_lock lock{ g_lock };
            a_set.insert(a_receiver->GetFormID());
        }

        void Remove(std::set<FormID>& a_set, RE::TESForm* a_receiver)
        {
            if (!a_receiver) {
                return;
            }
            std::unique_lock lock{ g_lock };
            a_set.erase(a_receiver->GetFormID());
        }
    }

    void RegisterPlayerDefeated(RE::TESForm* a_receiver) { Add(g_playerDefeated, a_receiver); }
    void UnregisterPlayerDefeated(RE::TESForm* a_receiver) { Remove(g_playerDefeated, a_receiver); }
    void RegisterPlayerRecovered(RE::TESForm* a_receiver) { Add(g_playerRecovered, a_receiver); }
    void UnregisterPlayerRecovered(RE::TESForm* a_receiver) { Remove(g_playerRecovered, a_receiver); }
    void RegisterPlayerConsequenceStarted(RE::TESForm* a_receiver) { Add(g_playerConsequenceStarted, a_receiver); }
    void UnregisterPlayerConsequenceStarted(RE::TESForm* a_receiver) { Remove(g_playerConsequenceStarted, a_receiver); }

    void BroadcastPlayerDefeated() { Broadcast(g_playerDefeated, "OnPlayerDefeated"); }
    void BroadcastPlayerRecovered() { Broadcast(g_playerRecovered, "OnPlayerRecovered"); }
    void BroadcastPlayerConsequenceStarted() { Broadcast(g_playerConsequenceStarted, "OnPlayerConsequenceStarted"); }

    void RegisterActorDefeated(RE::TESForm* a_receiver) { Add(g_actorDefeated, a_receiver); }
    void UnregisterActorDefeated(RE::TESForm* a_receiver) { Remove(g_actorDefeated, a_receiver); }
    void RegisterActorRecovered(RE::TESForm* a_receiver) { Add(g_actorRecovered, a_receiver); }
    void UnregisterActorRecovered(RE::TESForm* a_receiver) { Remove(g_actorRecovered, a_receiver); }

    void BroadcastActorDefeated(FormID a_victim, FormID a_aggressor) { BroadcastActors(g_actorDefeated, "OnActorDefeated", a_victim, a_aggressor); }
    void BroadcastActorRecovered(FormID a_victim, FormID a_aggressor) { BroadcastActors(g_actorRecovered, "OnActorRecovered", a_victim, a_aggressor); }

    void Install()
    {
        // Light up the dormant Defeat observers so every defeat/rescue (player AND NPC) fans out to
        // OnActorDefeated/OnActorRecovered receivers. Fires on the game thread (DefeatActor/RescueActor).
        Defeat::SetOnDefeat([](FormID a_victim, FormID a_aggressor) { BroadcastActorDefeated(a_victim, a_aggressor); });
        Defeat::SetOnRescue([](FormID a_victim, FormID a_aggressor) { BroadcastActorRecovered(a_victim, a_aggressor); });
    }

    void Clear()
    {
        std::unique_lock lock{ g_lock };
        g_playerDefeated.clear();
        g_playerRecovered.clear();
        g_playerConsequenceStarted.clear();
        g_actorDefeated.clear();
        g_actorRecovered.clear();
    }
}