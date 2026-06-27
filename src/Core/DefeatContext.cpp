#include "pch.h"

#include "Core/DefeatContext.h"

namespace OSFDefeat::DefeatContext
{
    namespace
    {
        PlayerContext g_playerContext;
        std::shared_mutex g_lock;
    }

    void SetPlayerContext(RE::Actor* a_player, RE::Actor* a_aggressor, const Consequence::PlayerOutcome& a_outcome)
    {
        PlayerContext next;
        next.player = a_player ? a_player->GetFormID() : 0x14;
        next.aggressor = a_aggressor ? a_aggressor->GetFormID() : 0;
        next.consequenceName = a_outcome.name;
        next.consequenceQuest = a_outcome.quest.value_or("");
        next.recoverySeconds = a_outcome.recoverySeconds;

        std::unique_lock lock{ g_lock };
        g_playerContext = std::move(next);
    }

    void ClearPlayerContext()
    {
        std::unique_lock lock{ g_lock };
        g_playerContext = {};
    }

    PlayerContext GetPlayerContext()
    {
        std::shared_lock lock{ g_lock };
        return g_playerContext;
    }

    RE::Actor* GetLastPlayerAggressor()
    {
        const auto context = GetPlayerContext();
        return context.aggressor ? RE::TESForm::LookupByID<RE::Actor>(context.aggressor) : nullptr;
    }

    std::string GetLastPlayerConsequenceName()
    {
        return GetPlayerContext().consequenceName;
    }

    std::string GetLastPlayerConsequenceQuest()
    {
        return GetPlayerContext().consequenceQuest;
    }

    float GetLastPlayerRecoverySeconds()
    {
        return GetPlayerContext().recoverySeconds;
    }
}