#include "pch.h"

#include "Core/DefeatContext.h"

namespace OSFDefeat::DefeatContext
{
    namespace
    {
        struct PlayerContext
        {
            FormID aggressor{ 0 };
            std::string consequenceId;
            std::string consequenceDisplayName;
            std::string consequenceQuest;
            float autoRecoveryDelaySeconds{ 0.0f };  // negative = held; zero = no context
            DefeatCause cause{ DefeatCause::kNone };
            std::vector<FormID> recentHostileAttackerIds;
        };

        PlayerContext g_playerContext;
        std::shared_mutex g_lock;
    }

    void SetPlayerContext(RE::Actor* a_aggressor, const Consequence::PlayerConsequence& a_consequence,
        const std::vector<RE::Actor*>& a_recentHostileAttackers, DefeatCause a_cause)
    {
        PlayerContext next;
        next.aggressor = a_aggressor ? a_aggressor->GetFormID() : 0;
        next.consequenceId = a_consequence.id;
        next.consequenceDisplayName = a_consequence.displayName;
        next.consequenceQuest = a_consequence.quest.value_or("");
        next.autoRecoveryDelaySeconds = a_consequence.HoldsRecovery() ? -1.0f : a_consequence.autoRecoveryDelaySeconds;
        next.cause = a_cause;
        next.recentHostileAttackerIds.reserve(a_recentHostileAttackers.size());
        for (auto* const attacker : a_recentHostileAttackers) {
            next.recentHostileAttackerIds.push_back(attacker->GetFormID());
        }

        std::unique_lock lock{ g_lock };
        g_playerContext = std::move(next);
    }

    void ClearPlayerContext()
    {
        std::unique_lock lock{ g_lock };
        g_playerContext = {};
    }

    RE::Actor* GetLastPlayerAggressor()
    {
        FormID aggressor;
        {
            std::shared_lock lock{ g_lock };
            aggressor = g_playerContext.aggressor;
        }
        return aggressor ? RE::TESForm::LookupByID<RE::Actor>(aggressor) : nullptr;
    }

    std::string GetLastPlayerConsequenceId()
    {
        std::shared_lock lock{ g_lock };
        return g_playerContext.consequenceId;
    }

    std::string GetLastPlayerConsequenceName()
    {
        std::shared_lock lock{ g_lock };
        return g_playerContext.consequenceDisplayName;
    }

    std::string GetLastPlayerConsequenceQuest()
    {
        std::shared_lock lock{ g_lock };
        return g_playerContext.consequenceQuest;
    }

    float GetLastPlayerAutoRecoveryDelaySeconds()
    {
        std::shared_lock lock{ g_lock };
        return g_playerContext.autoRecoveryDelaySeconds;
    }

    bool IsLastPlayerRecoveryHeld()
    {
        std::shared_lock lock{ g_lock };
        return g_playerContext.autoRecoveryDelaySeconds < 0.0f;
    }

    DefeatCause GetLastPlayerDefeatCause()
    {
        std::shared_lock lock{ g_lock };
        return g_playerContext.cause;
    }

    std::vector<RE::Actor*> GetLastPlayerRecentHostileAttackers()
    {
        std::vector<FormID> ids;
        {
            std::shared_lock lock{ g_lock };
            ids = g_playerContext.recentHostileAttackerIds;
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
}
