#include "pch.h"

#include "Core/Consequence.h"

#include "Core/Config.h"

#include <nlohmann/json.hpp>

namespace OSFDefeat::Consequence
{
    namespace
    {
        using namespace std::string_view_literals;

        std::vector<PlayerConsequence> g_playerConsequences;

        // Arbitration state, session-scoped by design: a cooldown or a spent cell does not survive
        // a game restart (documented). Keyed by stable consequence ID; guarded by g_lock.
        struct FiredState
        {
            std::chrono::steady_clock::time_point lastFired{};
            std::set<FormID> firedCells;
        };
        std::map<std::string, FiredState, std::less<>> g_fired;

        std::shared_mutex g_lock;

        // Author-facing typo guard: warn on unknown top-level keys. Keys starting with "//" are the
        // JSON-comment convention and stay silent (the parser also accepts real // comments).
        constexpr std::array kKnownConsequenceKeys = {
            "version"sv, "id"sv, "displayName"sv, "name"sv, "priority"sv, "weight"sv,
            "autoRecoveryDelaySeconds"sv, "recoverySeconds"sv, "quest"sv,
            "startQuest"sv, "recoveryMode"sv, "holdRecovery"sv, "leaseTimeoutSeconds"sv,
            "maxHoldSeconds"sv, "enabled"sv, "cooldownSeconds"sv,
            "oncePerLocation"sv, "domain"sv, "conditions"sv
        };

        void WarnUnknownKeys(const nlohmann::json& a_json, const std::filesystem::path& a_path)
        {
            for (const auto& [key, value] : a_json.items()) {
                if (key.starts_with("//")) {
                    continue;
                }
                if (std::find(kKnownConsequenceKeys.begin(), kKnownConsequenceKeys.end(), key) == kKnownConsequenceKeys.end()) {
                    REX::WARN("[OSFDefeat] consequence JSON {}: unknown field '{}' (typo?)", a_path.string(), key);
                }
            }
        }

        [[nodiscard]] std::optional<PlayerConsequence> ParseConsequence(const std::filesystem::path& a_path)
        {
            try {
                std::ifstream input{ a_path };
                const auto json = nlohmann::json::parse(input, nullptr, true, true);
                const int version = json.value("version", 0);
                if (version != 1 && version != 2 && version != 3) {
                    REX::WARN("[OSFDefeat] consequence JSON {} has unsupported version {}", a_path.string(), version);
                    return std::nullopt;
                }
                WarnUnknownKeys(json, a_path);

                const float defaultAutoRecoveryDelaySeconds = Config::DefaultAutoRecoveryDelaySeconds();
                PlayerConsequence consequence;
                const auto legacyName = json.value("name", a_path.stem().string());
                consequence.id = version >= 3 ? json.value("id", std::string{}) : legacyName;
                consequence.displayName = version >= 3 ? json.value("displayName", legacyName) : legacyName;
                if (consequence.id.empty()) {
                    REX::WARN("[OSFDefeat] consequence JSON {} has no stable id; file skipped", a_path.string());
                    return std::nullopt;
                }
                if (consequence.displayName.empty()) {
                    consequence.displayName = consequence.id;
                }
                consequence.priority = json.value("priority", 0);
                consequence.weight = std::max(1, json.value("weight", 1));
                const auto autoRecoveryDelaySeconds = json.contains("autoRecoveryDelaySeconds")
                    ? json.value("autoRecoveryDelaySeconds", defaultAutoRecoveryDelaySeconds)
                    : json.value("recoverySeconds", defaultAutoRecoveryDelaySeconds);
                if (!std::isfinite(autoRecoveryDelaySeconds)) {
                    throw std::runtime_error("autoRecoveryDelaySeconds must be finite");
                }
                consequence.autoRecoveryDelaySeconds = std::max(1.0f, autoRecoveryDelaySeconds);
                if (const auto it = json.find("quest"); it != json.end() && it->is_string() && !it->get<std::string>().empty()) {
                    consequence.quest = it->get<std::string>();
                }
                consequence.startQuest = json.value("startQuest", true);
                if (const auto it = json.find("recoveryMode"); it != json.end()) {
                    if (!it->is_string() || (it->get<std::string>() != "automatic" && it->get<std::string>() != "held")) {
                        throw std::runtime_error("recoveryMode must be automatic or held");
                    }
                    consequence.recoveryMode = it->get<std::string>() == "held" ? RecoveryMode::kHeld : RecoveryMode::kAutomatic;
                } else {
                    consequence.recoveryMode = json.value("holdRecovery", false) ? RecoveryMode::kHeld : RecoveryMode::kAutomatic;
                }
                consequence.leaseTimeoutSeconds = json.contains("leaseTimeoutSeconds")
                    ? json.value("leaseTimeoutSeconds", 0.0f)
                    : json.value("maxHoldSeconds", 0.0f);
                consequence.enabled = json.value("enabled", true);
                consequence.cooldownSeconds = json.value("cooldownSeconds", 0.0f);
                if (!std::isfinite(consequence.leaseTimeoutSeconds) || !std::isfinite(consequence.cooldownSeconds)) {
                    throw std::runtime_error("leaseTimeoutSeconds and cooldownSeconds must be finite");
                }
                consequence.oncePerLocation = json.value("oncePerLocation", false);
                if (const auto it = json.find("domain"); it != json.end() && it->is_string()) {
                    const auto domain = it->get<std::string>();
                    if (domain == "onfoot") {
                        consequence.domain = Domain::kOnFoot;
                    } else if (domain == "ship") {
                        consequence.domain = Domain::kShip;
                        REX::INFO("[OSFDefeat] consequence '{}' targets the reserved 'ship' domain; parked until ship defeat lands", consequence.id);
                    } else {
                        // Unknown domain: park it rather than misfire it on foot — a newer pack's
                        // domain should never run in the wrong context on an older framework.
                        consequence.domain = Domain::kShip;
                        REX::WARN("[OSFDefeat] consequence '{}': unknown domain '{}'; consequence will not compete", consequence.id, domain);
                    }
                }
                if (const auto it = json.find("conditions"); it != json.end()) {
                    consequence.conditions = Conditions::Parse(*it, consequence.id);
                }
                return consequence;
            } catch (const std::exception& e) {
                REX::ERROR("[OSFDefeat] consequence JSON {} failed: {}", a_path.string(), e.what());
                return std::nullopt;
            }
        }

        // Central user tuning (Acheron's Weights.yaml lesson): one file overrides enabled/weight/
        // priority per stable consequence ID, so users tune installed packs without editing pack files
        // (which mod managers overwrite on update).
        void ApplyOverrides(std::vector<PlayerConsequence>& a_consequences)
        {
            const auto path = Config::RootPath() / "Overrides.json";
            if (!std::filesystem::exists(path)) {
                return;
            }
            try {
                std::ifstream input{ path };
                const auto json = nlohmann::json::parse(input, nullptr, true, true);
                const int version = json.value("version", 0);
                if (version != 1 && version != 2) {
                    REX::WARN("[OSFDefeat] Overrides.json has unsupported version {}", version);
                    return;
                }
                const auto it = json.find("consequences");
                if (it == json.end() || !it->is_object()) {
                    return;
                }
                for (const auto& [key, entry] : it->items()) {
                    if (key.starts_with("//") || !entry.is_object()) {
                        continue;
                    }
                    const auto match = std::find_if(a_consequences.begin(), a_consequences.end(),
                        [&](const PlayerConsequence& a_consequence) {
                            return a_consequence.id == key || (version == 1 && a_consequence.displayName == key);
                        });
                    if (match == a_consequences.end()) {
                        REX::INFO("[OSFDefeat] Overrides.json names unknown consequence '{}' (not installed, or ID changed?)", key);
                        continue;
                    }
                    match->enabled = entry.value("enabled", match->enabled);
                    match->weight = std::max(1, entry.value("weight", match->weight));
                    match->priority = entry.value("priority", match->priority);
                    REX::INFO("[OSFDefeat] Overrides.json: '{}' -> enabled={} weight={} priority={}",
                        match->id, match->enabled, match->weight, match->priority);
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
        std::vector<PlayerConsequence> next;
        const auto root = Config::RootPath() / "Consequences" / "Player";
        try {
            if (std::filesystem::exists(root)) {
                std::vector<std::filesystem::path> files;
                for (const auto& entry : std::filesystem::directory_iterator{ root }) {
                    if (entry.is_regular_file() && entry.path().extension() == ".json") {
                        files.push_back(entry.path());
                    }
                }
                std::ranges::sort(files);
                std::set<std::string, std::less<>> ids;
                for (const auto& path : files) {
                    if (auto consequence = ParseConsequence(path)) {
                        if (!ids.insert(consequence->id).second) {
                            REX::WARN("[OSFDefeat] duplicate consequence ID '{}' in {}; later file skipped", consequence->id, path.string());
                            continue;
                        }
                        next.push_back(std::move(*consequence));
                    }
                }
            } else {
                REX::INFO("[OSFDefeat] no player consequence JSON directory at {}; using built-in fallback", root.string());
            }
        } catch (const std::exception& e) {
            REX::ERROR("[OSFDefeat] consequence directory discovery failed for {}: {}", root.string(), e.what());
        }
        ApplyOverrides(next);

        std::unique_lock lock{ g_lock };
        g_playerConsequences = std::move(next);
        REX::INFO("[OSFDefeat] loaded {} player consequence(s)", g_playerConsequences.size());
    }

    void Reload() { Load(); }

    PlayerConsequence SelectPlayerConsequence(RE::Actor* a_victim, RE::Actor* a_aggressor, DefeatCause a_cause)
    {
        const bool explain = Config::IsExplainEnabled();
        const FormID cell = CellOf(a_victim);
        const auto now = std::chrono::steady_clock::now();

        std::shared_lock lock{ g_lock };

        // Only consequences that pass every filter compete on priority and weight.
        // Log each filtered consequence's reason so authors can see why it didn't fire.
        std::vector<std::reference_wrapper<const PlayerConsequence>> pool;
        int bestPriority = 0;
        std::int64_t totalWeight = 0;
        for (const auto& consequence : g_playerConsequences) {
            std::string reason;
            if (!consequence.enabled) {
                reason = "disabled (enabled: false, or Overrides.json)";
            } else if (consequence.domain != Domain::kOnFoot) {
                reason = "off-domain (ship domain is reserved)";
            } else {
                const auto fired = g_fired.find(consequence.id);
                if (fired != g_fired.end()) {
                    if (consequence.cooldownSeconds > 0.0f && fired->second.lastFired.time_since_epoch().count() != 0) {
                        const auto since = std::chrono::duration_cast<std::chrono::seconds>(now - fired->second.lastFired).count();
                        if (since < static_cast<long long>(consequence.cooldownSeconds)) {
                            reason = std::format("cooling down ({}s of {:.0f}s elapsed)", since, consequence.cooldownSeconds);
                        }
                    }
                    if (reason.empty() && consequence.oncePerLocation && cell != 0 && fired->second.firedCells.contains(cell)) {
                        reason = "already fired in this location (oncePerLocation)";
                    }
                }
                if (reason.empty()) {
                    switch (Conditions::Explain(consequence.conditions, a_victim, a_aggressor, a_cause)) {
                    case Conditions::EvalResult::kInvalid:
                        reason = "invalid or unsupported authored conditions";
                        break;
                    case Conditions::EvalResult::kAggressorFailed:
                        reason = "aggressor conditions failed";
                        break;
                    case Conditions::EvalResult::kVictimFailed:
                        reason = "victim conditions failed";
                        break;
                    case Conditions::EvalResult::kCauseFailed:
                        reason = "cause conditions failed (defeat cause not in list)";
                        break;
                    default:
                        break;
                    }
                }
            }

            if (reason.empty()) {
                if (pool.empty() || consequence.priority > bestPriority) {
                    pool.clear();
                    bestPriority = consequence.priority;
                    totalWeight = 0;
                }
                if (consequence.priority == bestPriority) {
                    pool.emplace_back(consequence);
                    totalWeight += consequence.weight;
                }
            } else {
                REX::INFO("[OSFDefeat] consequence '{}' filtered out: {}", consequence.id, reason);
            }
        }
        if (pool.empty()) {
            REX::INFO("[OSFDefeat] no eligible consequence; using built-in down-then-recover fallback");
            PlayerConsequence fallback;
            fallback.id = "osfdefeat.builtin.down_then_recover";
            fallback.displayName = "Down Then Recover";
            fallback.autoRecoveryDelaySeconds = Config::DefaultAutoRecoveryDelaySeconds();
            return fallback;
        }

        static thread_local std::mt19937 rng{ std::random_device{}() };
        // The pool is nonempty and both JSON loaders normalize every weight to at least one.
        std::uniform_int_distribution<std::int64_t> dist{ 1, totalWeight };
        const auto roll = dist(rng);
        auto remaining = roll;
        const PlayerConsequence* selected = &pool.front().get();
        for (const auto& entry : pool) {
            const auto& consequence = entry.get();
            remaining -= consequence.weight;
            if (remaining <= 0) {
                selected = &consequence;
                break;
            }
        }

        if (explain) {
            std::string poolNames;
            for (const auto& entry : pool) {
                if (!poolNames.empty()) {
                    poolNames += ", ";
                }
                poolNames += std::format("'{}'(w{})", entry.get().id, entry.get().weight);
            }
            REX::INFO("[OSFDefeat] selection pool (priority {}): [{}]; roll {}/{} -> '{}'",
                bestPriority, poolNames, roll, totalWeight, selected->id);
        }
        return *selected;
    }

    void MarkFired(const PlayerConsequence& a_consequence, RE::Actor* a_victim)
    {
        if (a_consequence.cooldownSeconds <= 0.0f && !a_consequence.oncePerLocation) {
            return;
        }
        const FormID cell = CellOf(a_victim);
        std::unique_lock lock{ g_lock };
        auto& fired = g_fired[a_consequence.id];
        fired.lastFired = std::chrono::steady_clock::now();
        if (a_consequence.oncePerLocation && cell != 0) {
            fired.firedCells.insert(cell);
        }
    }

    void ResetRuntimeState()
    {
        std::unique_lock lock{ g_lock };
        g_fired.clear();
    }
}
