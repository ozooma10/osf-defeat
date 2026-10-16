#pragma once

#include "pch.h"

namespace OSFDefeat::QuestHandoff
{
    // Resolves a consequence quest token and dispatches Papyrus Quest.Start, with logging.
    // Returns false when resolution or dispatch fails.
    bool TryStartQuestToken(std::string_view a_token);
}
