#pragma once

#include "pch.h"

#include <nlohmann/json_fwd.hpp>

namespace OSFDefeat::Conditions
{
    // Condition grammar for consequence selection (Acheron-style). v1 covers actor
    // matching only: keyword and faction tests on the aggressor and the victim.
    // Aggressor blocks default to "any" semantics (at least one listed item matches),
    // victim blocks to "all" (every listed item matches) - Acheron's semantics -
    // overridable per block with "match": "any" | "all".
    //
    // Location and quest-state conditions are schema-reserved but UNSUPPORTED until
    // their engine seams are RE-proven for this CommonLibSF (GetCurrentLocation has
    // an unmapped AddrLib ID and crashes if called; TESQuest state natives are
    // unbound). Authoring them logs a warning and the block is ignored.

    struct ActorBlock
    {
        std::vector<std::string> keywords;  // form tokens: editorID or hex FormID
        std::vector<std::string> factions;  // membership via GetFactionRank >= 0
        bool matchAll{ false };

        [[nodiscard]] bool Empty() const { return keywords.empty() && factions.empty(); }
    };

    struct OutcomeConditions
    {
        ActorBlock aggressor;  // defaults to matchAll = false ("any")
        ActorBlock victim;     // defaults to matchAll = true ("all")

        [[nodiscard]] bool Empty() const { return aggressor.Empty() && victim.Empty(); }
    };

    // Parses the "conditions" object of a consequence JSON. a_owner is the
    // consequence name, used for warnings about unsupported/unknown fields.
    [[nodiscard]] OutcomeConditions Parse(const nlohmann::json& a_json, std::string_view a_owner);

    // Evaluates on the game thread (HasKeyword / GetFactionRank engine reads).
    // A non-empty aggressor block demands an aggressor: null aggressor fails it.
    // Unresolvable form tokens count as non-matches (and warn once per token).
    [[nodiscard]] bool Evaluate(const OutcomeConditions& a_conditions, RE::Actor* a_victim, RE::Actor* a_aggressor);

    // Evaluate, but say which block failed — the selection explain log names it.
    enum class EvalResult
    {
        kPass,
        kAggressorFailed,
        kVictimFailed
    };
    [[nodiscard]] EvalResult Explain(const OutcomeConditions& a_conditions, RE::Actor* a_victim, RE::Actor* a_aggressor);
}
