#include "pch.h"
#include "Core/Truce.h"
#include "Core/GameTime.h"
#include "Core/Recovery.h"
#include "Core/RuntimeMutation.h"
#include "Engine/Engine.h"

namespace OSFDefeat::Truce
{
    namespace
    {
        std::map<FormID, Record> g_records;
        std::shared_mutex g_lock;
    }

    bool Begin(RE::Actor& a_actor, float a_seconds)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!std::isfinite(a_seconds) || a_seconds <= 0 || a_seconds > 3600 ||
            !Engine::IsCaptiveFactionAvailable()) {
            return false;
        }
        Engine::ApplyCaptiveFaction(a_actor);
        if (!Engine::IsInCaptiveFaction(a_actor)) {
            return false;
        }
        const Record record{ a_actor.GetFormID(), Recovery::DeadlineForSeconds(a_seconds),
            Engine::HasCaptiveFactionApplied(a_actor) };
        {
            std::unique_lock lock{ g_lock };
            g_records[record.actorId] = record;
        }
        Engine::StopCombat(a_actor);
        REX::INFO("[OSFDefeat] recovery truce on {:08X} for {:.1f}s", record.actorId, a_seconds);
        return true;
    }

    bool End(RE::Actor& a_actor)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        bool owned = false;
        {
            std::unique_lock lock{ g_lock };
            const auto it = g_records.find(a_actor.GetFormID());
            if (it == g_records.end()) {
                return false;
            }
            owned = it->second.ownsFaction;
            g_records.erase(it);
        }
        if (owned) {
            Engine::RemoveCaptiveFaction(a_actor);
        }
        REX::INFO("[OSFDefeat] recovery truce ended on {:08X}", a_actor.GetFormID());
        return true;
    }

    bool IsActive(FormID a_actorId)
    {
        std::shared_lock lock{ g_lock };
        return g_records.contains(a_actorId);
    }

    std::vector<Record> Snapshot()
    {
        std::shared_lock lock{ g_lock };
        std::vector<Record> result;
        for (const auto& [id, record] : g_records) {
            result.push_back(record);
        }
        return result;
    }

    void Tick()
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        for (const auto& record : Snapshot()) {
            if (GameTime::NowDay() >= record.expiresAtDay) {
                if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(record.actorId)) {
                    End(*actor);
                }
                // Keep unresolved ownership so cleanup retries when the actor returns.
            }
        }
    }

    void EndAll()
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        for (const auto& record : Snapshot()) {
            if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(record.actorId)) {
                End(*actor);
            } else {
                // An emergency cleanup must not leave an unresolved truce renewed.
                std::unique_lock lock{ g_lock };
                g_records.at(record.actorId).expiresAtDay = 0;
            }
        }
    }

    void Restore(Record a_record)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        std::unique_lock lock{ g_lock };
        g_records.try_emplace(a_record.actorId, a_record);
    }

    void Reset()
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        std::unique_lock lock{ g_lock };
        g_records.clear();
    }
}
