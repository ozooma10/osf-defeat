#include "pch.h"

#include "Core/DefeatContext.h"

namespace OSFDefeat::DefeatContext
{
    namespace
    {
        PlayerContext g_playerContext;
        std::shared_mutex g_lock;
    }

    void SetPlayerContext(RE::Actor* a_player, RE::Actor* a_aggressor, const Consequence::PlayerConsequence& a_consequence,
        const std::vector<RE::Actor*>& a_recentHostileAttackers, DefeatCause a_cause)
    {
        PlayerContext next;
        next.player = a_player ? a_player->GetFormID() : 0x14;
        next.aggressor = a_aggressor ? a_aggressor->GetFormID() : 0;
        next.consequenceId = a_consequence.id;
        next.consequenceDisplayName = a_consequence.displayName;
        next.consequenceQuest = a_consequence.quest.value_or("");
        next.autoRecoveryDelaySeconds = a_consequence.HoldsRecovery() ? -1.0f : a_consequence.autoRecoveryDelaySeconds;
        next.recoveryHeld = a_consequence.HoldsRecovery();
        next.cause = a_cause;
        next.recentHostileAttackerIds.reserve(a_recentHostileAttackers.size());
        for (auto* const attacker : a_recentHostileAttackers) {
            if (attacker) {
                next.recentHostileAttackerIds.push_back(attacker->GetFormID());
            }
        }

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

    std::string GetLastPlayerConsequenceId()
    {
        return GetPlayerContext().consequenceId;
    }

    std::string GetLastPlayerConsequenceName()
    {
        return GetPlayerContext().consequenceDisplayName;
    }

    std::string GetLastPlayerConsequenceQuest()
    {
        return GetPlayerContext().consequenceQuest;
    }

    float GetLastPlayerRecoverySeconds()
    {
        return GetPlayerContext().autoRecoveryDelaySeconds;
    }

    float GetLastPlayerAutoRecoveryDelaySeconds()
    {
        return GetPlayerContext().autoRecoveryDelaySeconds;
    }

    bool IsLastPlayerRecoveryHeld()
    {
        return GetPlayerContext().recoveryHeld;
    }

    DefeatCause GetLastPlayerDefeatCause()
    {
        return GetPlayerContext().cause;
    }

    std::vector<RE::Actor*> GetLastPlayerRecentHostileAttackers()
    {
        const auto context = GetPlayerContext();
        std::vector<RE::Actor*> out;
        out.reserve(context.recentHostileAttackerIds.size());
        for (const auto id : context.recentHostileAttackerIds) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                out.push_back(actor);
            }
        }
        return out;
    }

    std::vector<RE::Actor*> GetLastPlayerParticipants()
    {
        return GetLastPlayerRecentHostileAttackers();
    }
}
