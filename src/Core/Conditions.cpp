#include "pch.h"

#include "Core/Conditions.h"

#include "Core/FormToken.h"

#include "RE/B/BGSKeyword.h"
#include "RE/T/TESFaction.h"

#include <nlohmann/json.hpp>

namespace OSFDefeat::Conditions
{
    namespace
    {
        // Warn once per unresolvable token so a typo'd keyword doesn't spam the log
        // on every defeat.
        std::set<std::string, std::less<>> g_warnedTokens;
        std::mutex g_warnedLock;

        void WarnUnresolved(std::string_view a_token, std::string_view a_kind)
        {
            std::unique_lock lock{ g_warnedLock };
            if (g_warnedTokens.emplace(a_token).second) {
                REX::WARN("[OSFDefeat] condition {} token '{}' resolves to nothing; treating as non-match", a_kind, a_token);
            }
        }

        bool ReadTokenList(const nlohmann::json& a_block, std::string_view a_key, std::vector<std::string>& a_out, std::string_view a_owner)
        {
            const auto it = a_block.find(a_key);
            if (it == a_block.end()) {
                return true;
            }
            if (!it->is_array()) {
                REX::WARN("[OSFDefeat] consequence '{}': conditions {} must be an array of strings", a_owner, a_key);
                return false;
            }
            bool valid = true;
            for (const auto& entry : *it) {
                if (entry.is_string() && !entry.get<std::string>().empty()) {
                    a_out.push_back(entry.get<std::string>());
                } else {
                    REX::WARN("[OSFDefeat] consequence '{}': conditions {} entry is not a string", a_owner, a_key);
                    valid = false;
                }
            }
            return valid;
        }

        [[nodiscard]] ActorBlock ParseActorBlock(
            const nlohmann::json& a_json, std::string_view a_owner, bool a_defaultMatchAll, bool& a_valid)
        {
            ActorBlock block;
            block.matchAll = a_defaultMatchAll;
            a_valid = ReadTokenList(a_json, "keywords", block.keywords, a_owner) && a_valid;
            a_valid = ReadTokenList(a_json, "factions", block.factions, a_owner) && a_valid;
            if (const auto it = a_json.find("match"); it != a_json.end()) {
                if (!it->is_string()) {
                    REX::WARN("[OSFDefeat] consequence '{}': conditions match must be \"any\" or \"all\"", a_owner);
                    a_valid = false;
                } else {
                    const auto mode = it->get<std::string>();
                    if (mode == "all") {
                        block.matchAll = true;
                    } else if (mode == "any") {
                        block.matchAll = false;
                    } else {
                        REX::WARN("[OSFDefeat] consequence '{}': conditions match must be \"any\" or \"all\"", a_owner);
                        a_valid = false;
                    }
                }
            }
            for (const auto& [key, _] : a_json.items()) {
                if (key != "keywords" && key != "factions" && key != "match" && !key.starts_with("//")) {
                    REX::WARN("[OSFDefeat] consequence '{}': unknown actor-condition field '{}' (typo?); consequence parked", a_owner, key);
                    a_valid = false;
                }
            }
            if (block.Empty()) {
                REX::WARN("[OSFDefeat] consequence '{}': authored actor-condition block has no valid tests; consequence parked", a_owner);
                a_valid = false;
            }
            return block;
        }

        // Runs every individual test in the block against the actor and folds per
        // matchAll. Each listed token is one test; unresolvable tokens are misses.
        [[nodiscard]] bool EvaluateActorBlock(const ActorBlock& a_block, RE::Actor* a_actor)
        {
            if (a_block.Empty()) {
                return true;
            }
            if (!a_actor) {
                return false;  // block demands an actor to test
            }

            bool anyHit = false;
            bool allHit = true;
            const auto fold = [&](bool a_hit) {
                anyHit = anyHit || a_hit;
                allHit = allHit && a_hit;
            };

            for (const auto& token : a_block.keywords) {
                auto* const keyword = ResolveFormToken<RE::BGSKeyword>(token);
                if (!keyword) {
                    WarnUnresolved(token, "keyword");
                }
                fold(keyword && a_actor->HasKeyword(keyword));
            }
            for (const auto& token : a_block.factions) {
                auto* const faction = ResolveFormToken<RE::TESFaction>(token);
                if (!faction) {
                    WarnUnresolved(token, "faction");
                }
                fold(faction && a_actor->GetFactionRank(faction, a_actor->IsPlayerRef()) >= 0);
            }
            return a_block.matchAll ? allHit : anyHit;
        }
    }

    ConsequenceConditions Parse(const nlohmann::json& a_json, std::string_view a_consequenceId)
    {
        ConsequenceConditions conditions;
        if (!a_json.is_object()) {
            REX::WARN("[OSFDefeat] consequence '{}': conditions must be an object", a_consequenceId);
            conditions.valid = false;
            return conditions;
        }

        if (const auto it = a_json.find("aggressor"); it != a_json.end()) {
            if (it->is_object()) {
                conditions.aggressor = ParseActorBlock(*it, a_consequenceId, /*defaultMatchAll=*/false, conditions.valid);
            } else {
                REX::WARN("[OSFDefeat] consequence '{}': conditions.aggressor must be an object; consequence parked", a_consequenceId);
                conditions.valid = false;
            }
        }
        if (const auto it = a_json.find("victim"); it != a_json.end()) {
            if (it->is_object()) {
                conditions.victim = ParseActorBlock(*it, a_consequenceId, /*defaultMatchAll=*/true, conditions.valid);
            } else {
                REX::WARN("[OSFDefeat] consequence '{}': conditions.victim must be an object; consequence parked", a_consequenceId);
                conditions.valid = false;
            }
        }
        if (const auto it = a_json.find("cause"); it != a_json.end()) {
            const auto addCause = [&](const nlohmann::json& a_entry) {
                if (!a_entry.is_string()) {
                    REX::WARN("[OSFDefeat] consequence '{}': conditions cause entries must be strings", a_consequenceId);
                    conditions.valid = false;
                    return;
                }
                const auto token = a_entry.get<std::string>();
                if (token == "health") {
                    conditions.causes.push_back(DefeatCause::kHealth);
                } else if (token == "scripted") {
                    conditions.causes.push_back(DefeatCause::kScripted);
                } else {
                    REX::WARN("[OSFDefeat] consequence '{}': unknown cause '{}' (use health/scripted); consequence parked", a_consequenceId, token);
                    conditions.valid = false;
                }
            };
            if (it->is_array()) {
                for (const auto& entry : *it) {
                    addCause(entry);
                }
            } else {
                addCause(*it);
            }
            if (conditions.causes.empty()) {
                REX::WARN("[OSFDefeat] consequence '{}': conditions.cause has no valid entries; consequence parked", a_consequenceId);
                conditions.valid = false;
            }
        }

        // Schema-reserved, engine seams not yet RE-proven for this CommonLibSF.
        for (const auto key : { "locations", "questsRunning", "questsCompleted" }) {
            if (a_json.contains(key)) {
                REX::WARN("[OSFDefeat] consequence '{}': conditions.{} is not supported yet (engine seam unproven); consequence parked", a_consequenceId, key);
                conditions.valid = false;
            }
        }
        for (const auto& [key, _] : a_json.items()) {
            if (key != "aggressor" && key != "victim" && key != "cause" && key != "locations" &&
                key != "questsRunning" && key != "questsCompleted" && !key.starts_with("//")) {
                REX::WARN("[OSFDefeat] consequence '{}': unknown conditions field '{}' (typo?); consequence parked", a_consequenceId, key);
                conditions.valid = false;
            }
        }
        return conditions;
    }

    bool Evaluate(const ConsequenceConditions& a_conditions, RE::Actor* a_victim, RE::Actor* a_aggressor, DefeatCause a_cause)
    {
        return Explain(a_conditions, a_victim, a_aggressor, a_cause) == EvalResult::kPass;
    }

    EvalResult Explain(const ConsequenceConditions& a_conditions, RE::Actor* a_victim, RE::Actor* a_aggressor, DefeatCause a_cause)
    {
        if (!a_conditions.valid) {
            return EvalResult::kInvalid;
        }
        if (!a_conditions.causes.empty() &&
            std::find(a_conditions.causes.begin(), a_conditions.causes.end(), a_cause) == a_conditions.causes.end()) {
            return EvalResult::kCauseFailed;
        }
        if (!EvaluateActorBlock(a_conditions.aggressor, a_aggressor)) {
            return EvalResult::kAggressorFailed;
        }
        if (!EvaluateActorBlock(a_conditions.victim, a_victim)) {
            return EvalResult::kVictimFailed;
        }
        return EvalResult::kPass;
    }
}
