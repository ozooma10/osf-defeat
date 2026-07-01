#include "pch.h"

#include "Core/Consequence.h"

#include "Core/Config.h"

#include <nlohmann/json.hpp>

namespace OSFDefeat::Consequence
{
    namespace
    {
        std::vector<PlayerOutcome> g_playerOutcomes;
        std::shared_mutex g_lock;

        [[nodiscard]] std::optional<PlayerOutcome> ParseOutcome(const std::filesystem::path& a_path)
        {
            try {
                std::ifstream input{ a_path };
                const auto json = nlohmann::json::parse(input, nullptr, true, true);
                if (json.value("version", 0) != 1) {
                    REX::WARN("[OSFDefeat] consequence JSON {} has unsupported version", a_path.string());
                    return std::nullopt;
                }

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
                if (const auto it = json.find("conditions"); it != json.end()) {
                    outcome.conditions = Conditions::Parse(*it, outcome.name);
                }
                return outcome;
            } catch (const std::exception& e) {
                REX::ERROR("[OSFDefeat] consequence JSON {} failed: {}", a_path.string(), e.what());
                return std::nullopt;
            }
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

        std::unique_lock lock{ g_lock };
        g_playerOutcomes = std::move(next);
        REX::INFO("[OSFDefeat] loaded {} player consequence(s)", g_playerOutcomes.size());
    }

    void Reload() { Load(); }

    PlayerOutcome SelectPlayerOutcome(RE::Actor* a_victim, RE::Actor* a_aggressor)
    {
        std::shared_lock lock{ g_lock };

        // Condition gate first: only outcomes whose conditions pass for this pair
        // compete on priority/weight at all.
        std::vector<std::reference_wrapper<const PlayerOutcome>> eligible;
        for (const auto& outcome : g_playerOutcomes) {
            if (Conditions::Evaluate(outcome.conditions, a_victim, a_aggressor)) {
                eligible.emplace_back(outcome);
            } else {
                REX::INFO("[OSFDefeat] outcome '{}' filtered out by conditions", outcome.name);
            }
        }
        if (eligible.empty()) {
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
        int roll = dist(rng);
        for (const auto& entry : pool) {
            const auto& outcome = entry.get();
            roll -= std::max(1, outcome.weight);
            if (roll <= 0) {
                return outcome;
            }
        }
        return pool.front().get();
    }

    float DefaultRecoverySeconds()
    {
        std::shared_lock lock{ g_lock };
        return g_playerOutcomes.empty() ? Config::Get().defaultRecoverySeconds : g_playerOutcomes.front().recoverySeconds;
    }
}