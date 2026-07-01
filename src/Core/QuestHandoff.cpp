#include "pch.h"

#include "Core/QuestHandoff.h"

#include "Core/FormToken.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IVirtualMachine.h"
#include "RE/T/TESQuest.h"

namespace OSFDefeat::QuestHandoff
{
    namespace
    {
        bool NoArgs(RE::BSScrapArray<RE::BSScript::Variable>&)
        {
            return true;
        }
    }

    RE::TESQuest* ResolveQuest(std::string_view a_token)
    {
        return ResolveFormToken<RE::TESQuest>(a_token);
    }

    bool StartQuest(RE::TESQuest* a_quest)
    {
        if (!a_quest) {
            return false;
        }

        const auto* const gameVM = RE::GameVM::GetSingleton();
        auto* const vm = gameVM ? gameVM->GetVM() : nullptr;
        if (!vm) {
            REX::WARN("[OSFDefeat] Papyrus VM unavailable; cannot start consequence quest {:08X}", a_quest->GetFormID());
            return false;
        }

        const auto& handles = vm->GetObjectHandlePolicy();
        const auto handle = handles.GetHandleForObject(
            RE::BSScript::GetVMTypeID<RE::TESQuest>(),
            static_cast<const void*>(a_quest));
        if (handle == handles.EmptyHandle()) {
            REX::WARN("[OSFDefeat] no VM handle for consequence quest {:08X}", a_quest->GetFormID());
            return false;
        }

        const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> noCallback{};
        return vm->DispatchMethodCall(handle, "Quest", "Start", &NoArgs, noCallback, 0);
    }

    bool TryStartQuestToken(std::string_view a_token)
    {
        auto* const quest = ResolveQuest(a_token);
        if (!quest) {
            REX::INFO("[OSFDefeat] quest token '{}' does not resolve to a quest; leaving it to content consumers", a_token);
            return false;
        }
        if (!StartQuest(quest)) {
            REX::WARN("[OSFDefeat] failed to dispatch Quest.Start for '{}' ({:08X})", a_token, quest->GetFormID());
            return false;
        }
        REX::INFO("[OSFDefeat] dispatched Quest.Start for consequence quest '{}' ({:08X})", a_token, quest->GetFormID());
        return true;
    }
}
