#include "pch.h"

#include "Core/Events.h"

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

    void BroadcastPlayerDefeated() { Broadcast(g_playerDefeated, "OnPlayerDefeated"); }
    void BroadcastPlayerRecovered() { Broadcast(g_playerRecovered, "OnPlayerRecovered"); }

    void Clear()
    {
        std::unique_lock lock{ g_lock };
        g_playerDefeated.clear();
        g_playerRecovered.clear();
    }
}