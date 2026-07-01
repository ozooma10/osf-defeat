#pragma once

#include "pch.h"

namespace OSFDefeat::QuestHandoff
{
    // Resolves a consequence `quest` token to a TESQuest: editorID first, then a FormID. Returns nullptr when the token names no quest
    [[nodiscard]] RE::TESQuest* ResolveQuest(std::string_view a_token);

    // Starts the quest by dispatching Papyrus `Quest.Start` on the form's VM handle.
    bool StartQuest(RE::TESQuest* a_quest);

    // ResolveQuest + StartQuest with logging; no-op (false) on an unresolvable token.
    bool TryStartQuestToken(std::string_view a_token);
}
