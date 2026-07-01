#include "pch.h"

#include "Core/Consequence.h"

#include "Core/Config.h"

#include <nlohmann/json.hpp>

namespace OSFDefeat::Consequence
{
    namespace
    {
        using namespace std::string_view_literals;

        std::vector<PlayerOutcome> g_playerOutcomes;

        // Arbitration state, session-scoped by design: a cooldown or a spent cell does not survive
        // a game restart (documented). Keyed by outcome name; guarded by g_lock with the outcomes.
        struct FiredState
        {
            std::chrono::steady_clock::time_point lastFired{};
            std::set<FormID> firedCells;
        };
        std::map<std::string, FiredState, std::less<>> g_fired;

        std::shared_mutex g_lock;

        // Author-facing typo guard: warn on unknown top-level keys. Keys starting with "//" are the
        // JSON-comment convention and stay silent (the parser also accepts real // comments).
        constexpr std::array kKnownOutcomeKeys = {
            "version"sv, "name"sv, "priority"sv, "weight"sv, "recoverySeconds"sv, "quest"sv,
            "startQuest"sv, "holdRecovery"sv, "maxHoldSeconds"sv, "enabled"sv, "cooldownSeconds"sv,
            "oncePerLocation"sv, "domain"sv, "conditions"sv
        };

        void WarnUnknownKeys(const nlohmann::json& a_json, const std::filesystem::path& a_path)
        {
            for (const auto& [key, value] : a_json.items()) {
                if (key.starts_with("//")) {
                    continue;
                }
                if (std::find(kKnownOutcomeKeys.begin(), kKnownOutcomeKeys.end(), key) == kKnownOutcomeKeys.end()) {
                    REX::WARN("[OSFDefeat] consequence JSON {}: unknown field '{}' (typo?)", a_path.string(), key);
                }
            }
        }

        [[nodiscard]] std::optional<PlayerOutcome> ParseOutcome(const std::filesystem::path& a_path)
        {
            try {
                std::ifstream input{ a_path };
                const auto json = nlohmann::json::parse(input, nullptr, true, true);
                const int version = json.value("version", 0);
                if (version != 1 && version != 2) {
                    REX::WARN("[OSFDefeat] consequence JSON {} has unsupported version {}", a_path.string(), version);
                    return std::nullopt;
                }
                WarnUnknownKeys(json, a_path);

                const auto config = Config::Get();
                PlayerOutcome outcome;
                outcome.name = json.value("name", a_path.stem().string());
                outcome.priority = json.value("priority", 0);
                outcome.weight = std::max(1, json.value("weight", 1));
                outcome.recoverySeconds = std::max(1.0f, json.value("recoverySeconds", config.defaultRecoverySeconds));
                if (const auto it = json.find("quest"); it != json.end() && it->is_string() && !it->get<std::string>().empty()) {
                    outcome.quest = it->get<std::string>();
                }
                outcome.startQuest = json.value("startQuest", true);
                outcome.holdRecovery = json.value("holdRecovery", false);
                outcome.maxHoldSeconds = json.value("maxHoldSeconds", 0.0f);
                outcome.enabled = json.value("enabled", true);
                outcome.cooldownSeconds = json.value("cooldownSeconds", 0.0f);
                outcome.oncePerLocation = json.value("oncePerLocation", false);
                if (const auto it = json.find("domain"); it != json.end() && it->is_string()) {
                    const auto domain = it->get<std::string>();
                    if (domain == "onfoot") {
                        outcome.domain = Domain::kOnFoot;
                    } else if (domain == "ship") {
                        outcome.domain = Domain::kShip;
                        REX::INFO("[OSFDefeat] consequence '{}' targets the reserved 'ship' domain; parked until ship defeat lands", outcome.name);
                    } else {
                        // Unknown domain: park it rather than misfire it on foot — a newer pack's
                        // domain should never run in the wrong context on an older framework.
                        outcome.domain = Domain::kShip;
                        REX::WARN("[OSFDefeat] consequence '{}': unknown domain '{}'; outcome will not compete", outcome.name, domain);
                    }
                }
                if (const auto it = json.find("conditions"); it != json.end()) {
                    outcome.conditions = Conditions::Parse(*it, outcome.name);
                }
                return outcome;
            } catch (const std::exception& e) {
                REX::ERROR("[OSFDefeat] consequence JSON {} failed: {}", a_path.string(), e.what());
                return std::nullopt;
            }
        }

        // Central user tuning (Acheron's Weights.yaml lesson): one file overrides enabled/weight/
        // priority per consequence name, so users tune installed packs without editing pack files
        // (which mod managers overwrite on update).
        void ApplyOverrides(std::vector<PlayerOutcome>& a_outcomes)
        {
            const auto path = Config::RootPath() / "Overrides.json";
            if (!std::filesystem::exists(path)) {
                return;
            }
            try {
                std::ifstream input{ path };
                const auto json = nlohmann::json::parse(input, nullptr, true, true);
                const int version = json.value("version", 0);
                if (version != 1) {
                    REX::WARN("[OSFDefeat] Overrides.json has unsupported version {}", version);
                    return;
                }
                const auto it = json.find("consequences");
                if (it == json.end() || !it->is_object()) {
                    return;
                }
                for (const auto& [name, entry] : it->items()) {
                    if (name.starts_with("//") || !entry.is_object()) {
                        continue;
                    }
                    const auto match = std::find_if(a_outcomes.begin(), a_outcomes.end(),
                        [&](const PlayerOutcome& a_outcome) { return a_outcome.name == name; });
                    if (match == a_outcomes.end()) {
                        REX::INFO("[OSFDefeat] Overrides.json names unknown consequence '{}' (not installed, or renamed?)", name);
                        continue;
                    }
                    if (entry.contains("enabled")) {
                        match->enabled = entry.value("enabled", match->enabled);
                    }
                    if (entry.contains("weight")) {
                        match->weight = std::max(1, entry.value("weight", match->weight));
                    }
                    if (entry.contains("priority")) {
                        match->priority = entry.value("priority", match->priority);
                    }
                    REX::INFO("[OSFDefeat] Overrides.json: '{}' -> enabled={} weight={} priority={}",
                        name, match->enabled, match->weight, match->priority);
                }
            } catch (const std::exception& e) {
                REX::ERROR("[OSFDefeat] Overrides.json failed: {}", e.what());
            }
        }

        [[nodiscard]] FormID CellOf(RE::Actor* a_actor)
        {
            return a_actor && a_actor->parentCell ? a_actor->parentCell->GetFormID() : 0;
        }
    }

    void Load()
    {
        std::vector<PlayerOutcome> next;
        const auto root = Config::RootPath() / "Consequences" / "Player";
        if (std::filesystem::exists(root)) {
            for (const auto& entry : std::filesystem::directory_iterator{ root }) {
                if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                    continue;
                }
                if (auto outcome = ParseOutcome(entry.path())) {
                    next.push_back(std::move(*outcome));
                }
            }
        } else {
            REX::INFO("[OSFDefeat] no player consequence JSON directory at {}; using built-in fallback", root.string());
        }
        ApplyOverrides(next);

        std::unique_lock lock{ g_lock };
        g_playerOutcomes = std::move(next);
        REX::INFO("[OSFDefeat] loaded {} player consequence(s)", g_playerOutcomes.size());
    }

    void Reload() { Load(); }

    PlayerOutcome SelectPlayerOutcome(RE::Actor* a_victim, RE::Actor* a_aggressor)
    {
        const bool explain = Config::IsExplainEnabled();
        const FormID cell = CellOf(a_victim);
        const auto now = std::chrono::steady_clock::now();

        std::shared_lock lock{ g_lock };

        // Arbitration + condition gate first: only outcomes that survive every filter compete on
        // priority/weight at all. Each filtered outcome logs its reason — defeats are rare and this
        // line is how an author answers "why didn't my consequence fire".
        std::vector<std::reference_wrapper<const PlayerOutcome>> eligible;
        for (const auto& outcome : g_playerOutcomes) {
            std::string reason;
            if (!outcome.enabled) {
                reason = "disabled (enabled: false, or Overrides.json)";
            } else if (outcome.domain != Domain::kOnFoot) {
                reason = "off-domain (ship domain is reserved)";
            } else {
                const auto fired = g_fired.find(outcome.name);
                if (fired != g_fired.end()) {
                    if (outcome.cooldownSeconds > 0.0f && fired->second.lastFired.time_since_epoch().count() != 0) {
                        const auto since = std::chrono::duration_cast<std::chrono::seconds>(now - fired->second.lastFired).count();
                        if (since < static_cast<long long>(outcome.cooldownSeconds)) {
                            reason = std::format("cooling down ({}s of {:.0f}s elapsed)", since, outcome.cooldownSeconds);
                        }
                    }
                    if (reason.empty() && outcome.oncePerLocation && cell != 0 && fired->second.firedCells.contains(cell)) {
                        reason = "already fired in this location (oncePerLocation)";
                    }
                }
                if (reason.empty()) {
                    switch (Conditions::Explain(outcome.conditions, a_victim, a_aggressor)) {
                    case Conditions::EvalResult::kAggressorFailed:
                        reason = "aggressor conditions failed";
                        break;
                    case Conditions::EvalResult::kVictimFailed:
                        reason = "victim conditions failed";
                        break;
                    default:
                        break;
                    }
                }
            }

            if (reason.empty()) {
                eligible.emplace_back(outcome);
            } else {
                REX::INFO("[OSFDefeat] outcome '{}' filtered out: {}", outcome.name, reason);
            }
        }
        if (eligible.empty()) {
            REX::INFO("[OSFDefeat] no eligible outcome; using built-in down-then-recover fallback");
            PlayerOutcome fallback;
            fallback.recoverySeconds = Config::Get().defaultRecoverySeconds;
            return fallback;
        }

        const auto best = std::max_element(eligible.begin(), eligible.end(), [](const auto& a_lhs, const auto& a_rhs) {
            return a_lhs.get().priority < a_rhs.get().priority;
        });
        const int bestPriority = best->get().priority;

        std::vector<std::reference_wrapper<const PlayerOutcome>> pool;
        int totalWeight = 0;
        for (const auto& entry : eligible) {
            const auto& outcome = entry.get();
            if (outcome.priority == bestPriority) {
                pool.emplace_back(outcome);
                totalWeight += std::max(1, outcome.weight);
            }
        }

        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution dist{ 1, std::max(1, totalWeight) };
        const int roll = dist(rng);
        int remaining = roll;
        const PlayerOutcome* selected = &pool.front().get();
        for (const auto& entry : pool) {
            const auto& outcome = entry.get();
            remaining -= std::max(1, outcome.weight);
            if (remaining <= 0) {
                selected = &outcome;
                break;
            }
        }

        if (explain) {
            std::string poolNames;
            for (const auto& entry : pool) {
                if (!poolNames.empty()) {
                    poolNames += ", ";
                }
                poolNames += std::format("'{}'(w{})", entry.get().name, entry.get().weight);
            }
            REX::INFO("[OSFDefeat] selection pool (priority {}): [{}]; roll {}/{} -> '{}'",
                bestPriority, poolNames, roll, totalWeight, selected->name);
        }
        return *selected;
    }

    void MarkFired(const PlayerOutcome& a_outcome, RE::Actor* a_victim)
    {
        if (a_outcome.cooldownSeconds <= 0.0f && !a_outcome.oncePerLocation) {
            return;
        }
        const FormID cell = CellOf(a_victim);
        std::unique_lock lock{ g_lock };
        auto& fired = g_fired[a_outcome.name];
        fired.lastFired = std::chrono::steady_clock::now();
        if (a_outcome.oncePerLocation && cell != 0) {
            fired.firedCells.insert(cell);
        }
    }

    float DefaultRecoverySeconds()
    {
        std::shared_lock lock{ g_lock };
        return g_playerOutcomes.empty() ? Config::Get().defaultRecoverySeconds : g_playerOutcomes.front().recoverySeconds;
    }
}
