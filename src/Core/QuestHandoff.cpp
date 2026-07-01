#include "pch.h"

#include "Core/QuestHandoff.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IVirtualMachine.h"
#include "RE/T/TESQuest.h"

namespace OSFDefeat::QuestHandoff
{
    namespace
    {
        [[nodiscard]] std::optional<std::uint32_t> ParseHexFormID(std::string_view a_token)
        {
            if (a_token.starts_with("0x") || a_token.starts_with("0X")) {
                a_token.remove_prefix(2);
            }
            if (a_token.empty() || a_token.size() > 8) {
                return std::nullopt;
            }
            std::uint32_t parsed = 0;
            const auto [ptr, ec] = std::from_chars(a_token.data(), a_token.data() + a_token.size(), parsed, 16);
            return ec == std::errc{} && ptr == a_token.data() + a_token.size() ? std::optional{ parsed } : std::nullopt;
        }

        bool NoArgs(RE::BSScrapArray<RE::BSScript::Variable>&)
        {
            return true;
        }
    }

    RE::TESQuest* ResolveQuest(std::string_view a_token)
    {
        if (a_token.empty()) {
            return nullptr;
        }
        if (auto* const quest = RE::TESForm::LookupByEditorID<RE::TESQuest>(RE::BSFixedString{ a_token })) {
            return quest;
        }
        if (const auto id = ParseHexFormID(a_token)) {
            return RE::TESForm::LookupByID<RE::TESQuest>(*id);
        }
        return nullptr;
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
