#include "pch.h"

#include "Core/DefeatContext.h"

namespace OSFDefeat::DefeatContext
{
    namespace
    {
        PlayerContext g_playerContext;
        std::shared_mutex g_lock;
    }

    void SetPlayerContext(RE::Actor* a_player, RE::Actor* a_aggressor, const Consequence::PlayerOutcome& a_outcome,
        const std::vector<RE::Actor*>& a_participants)
    {
        PlayerContext next;
        next.player = a_player ? a_player->GetFormID() : 0x14;
        next.aggressor = a_aggressor ? a_aggressor->GetFormID() : 0;
        next.consequenceName = a_outcome.name;
        next.consequenceQuest = a_outcome.quest.value_or("");
        next.recoverySeconds = a_outcome.recoverySeconds;
        next.participants.reserve(a_participants.size());
        for (auto* const participant : a_participants) {
            if (participant) {
                next.participants.push_back(participant->GetFormID());
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

    std::vector<RE::Actor*> GetLastPlayerParticipants()
    {
        const auto context = GetPlayerContext();
        std::vector<RE::Actor*> out;
        out.reserve(context.participants.size());
        for (const auto id : context.participants) {
            if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                out.push_back(actor);
            }
        }
        return out;
    }
}