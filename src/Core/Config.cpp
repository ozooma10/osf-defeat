#include "pch.h"

#include "Core/Config.h"

#include <nlohmann/json.hpp>

namespace OSFDefeat::Config
{
    namespace
    {
        RuntimeConfig g_config;
        std::filesystem::path g_root;
        std::shared_mutex g_lock;

        [[nodiscard]] std::optional<FormID> ParseFormID(const nlohmann::json& a_value)
        {
            if (a_value.is_number_unsigned()) {
                return a_value.get<FormID>();
            }
            if (a_value.is_number_integer()) {
                const auto raw = a_value.get<std::int64_t>();
                if (raw >= 0 && raw <= std::numeric_limits<FormID>::max()) {
                    return static_cast<FormID>(raw);
                }
                return std::nullopt;
            }
            if (!a_value.is_string()) {
                return std::nullopt;
            }

            auto text = a_value.get<std::string>();
            if (text.starts_with("0x") || text.starts_with("0X")) {
                text.erase(0, 2);
            }
            FormID parsed = 0;
            const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), parsed, 16);
            return ec == std::errc{} && ptr == text.data() + text.size() ? std::optional<FormID>{ parsed } : std::nullopt;
        }

        void ReadFormSet(const nlohmann::json& a_root, std::string_view a_key, std::set<FormID>& a_out)
        {
            const auto it = a_root.find(a_key);
            if (it == a_root.end() || !it->is_array()) {
                return;
            }

            for (const auto& entry : *it) {
                if (const auto id = ParseFormID(entry)) {
                    a_out.insert(*id);
                } else {
                    REX::WARN("[OSFDefeat] validation {} entry is not a numeric formID", a_key);
                }
            }
        }

        [[nodiscard]] std::vector<std::filesystem::path> CandidateRoots()
        {
            std::vector<std::filesystem::path> roots;
            const auto cwd = std::filesystem::current_path();
            roots.push_back(cwd / "Data" / "SFSE" / "OSFDefeat");
            roots.push_back(cwd / "SFSE" / "OSFDefeat");
            roots.push_back(cwd / "dist" / "SFSE" / "OSFDefeat");

            if (const char* const gameRoot = std::getenv("XSE_SF_GAME_PATH"); gameRoot && *gameRoot) {
                roots.emplace_back(std::filesystem::path{ gameRoot } / "Data" / "SFSE" / "OSFDefeat");
            }
            if (const char* const modsRoot = std::getenv("XSE_SF_MODS_PATH"); modsRoot && *modsRoot) {
                roots.emplace_back(std::filesystem::path{ modsRoot } / "OSF Defeat" / "SFSE" / "OSFDefeat");
            }
            return roots;
        }

        [[nodiscard]] std::filesystem::path ResolveRoot()
        {
            for (const auto& root : CandidateRoots()) {
                if (std::filesystem::exists(root)) {
                    return root;
                }
            }
            return std::filesystem::current_path() / "Data" / "SFSE" / "OSFDefeat";
        }

        void LoadFile(const std::filesystem::path& a_path, RuntimeConfig& a_config)
        {
            try {
                std::ifstream input{ a_path };
                const auto json = nlohmann::json::parse(input, nullptr, true, true);
                const int version = json.value("version", 0);
                if (version != 1 && version != 2) {
                    REX::WARN("[OSFDefeat] validation JSON {} has unsupported version {}", a_path.string(), version);
                    return;
                }

                if (json.contains("processingEnabled")) {
                    a_config.processingEnabled = json.value("processingEnabled", true);
                }
                if (json.contains("disableInScenes")) {
                    a_config.disableInScenes = json.value("disableInScenes", false);
                }
                if (json.contains("defaultRecoverySeconds")) {
                    a_config.defaultRecoverySeconds = std::max(1.0f, json.value("defaultRecoverySeconds", 8.0f));
                }
                if (json.contains("npcAutoDefeat")) {
                    a_config.npcAutoDefeat = json.value("npcAutoDefeat", false);
                }
                if (json.contains("npcAutoDefeatIncludeRobots")) {
                    a_config.npcAutoDefeatIncludeRobots = json.value("npcAutoDefeatIncludeRobots", false);
                }
                if (json.contains("factionPacify")) {
                    a_config.factionPacify = json.value("factionPacify", false);
                }
                if (const auto it = json.find("captiveFaction"); it != json.end()) {
                    if (const auto id = ParseFormID(*it)) {
                        a_config.captiveFaction = *id;
                    } else {
                        REX::WARN("[OSFDefeat] captiveFaction is not a numeric formID; keeping default");
                    }
                }
                if (json.contains("participantScanRadius")) {
                    a_config.participantScanRadius = json.value("participantScanRadius", 4096.0f);
                }
                if (json.contains("explainDecisions")) {
                    a_config.explainDecisions = json.value("explainDecisions", false);
                }
                if (json.contains("watchdogSeconds")) {
                    a_config.watchdogSeconds = json.value("watchdogSeconds", 600.0f);
                }
                ReadFormSet(json, "excludedLocations", a_config.excludedLocations);
                ReadFormSet(json, "excludedQuests", a_config.excludedQuests);
                REX::INFO("[OSFDefeat] loaded validation JSON {}", a_path.string());
            } catch (const std::exception& e) {
                REX::ERROR("[OSFDefeat] validation JSON {} failed: {}", a_path.string(), e.what());
            }
        }
    }

    void Load()
    {
        RuntimeConfig next;
        const auto root = ResolveRoot();
        const auto validationRoot = root / "Validation";

        if (std::filesystem::exists(validationRoot)) {
            for (const auto& entry : std::filesystem::directory_iterator{ validationRoot }) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    LoadFile(entry.path(), next);
                }
            }
        } else {
            REX::INFO("[OSFDefeat] no validation JSON directory at {}; using defaults", validationRoot.string());
        }

        std::unique_lock lock{ g_lock };
        g_config = std::move(next);
        g_root = root;
    }

    void Reload() { Load(); }

    RuntimeConfig Get()
    {
        std::shared_lock lock{ g_lock };
        return g_config;
    }

    bool IsProcessingEnabled()
    {
        std::shared_lock lock{ g_lock };
        return g_config.processingEnabled;
    }

    bool IsLocationExcluded(RE::BGSLocation* a_location)
    {
        if (!a_location) {
            return false;
        }
        std::shared_lock lock{ g_lock };
        return g_config.excludedLocations.contains(a_location->GetFormID());
    }

    bool HasExcludedLocations()
    {
        std::shared_lock lock{ g_lock };
        return !g_config.excludedLocations.empty();
    }

    bool ShouldDisableInScenes()
    {
        std::shared_lock lock{ g_lock };
        return g_config.disableInScenes;
    }

    bool IsNpcAutoDefeatEnabled()
    {
        std::shared_lock lock{ g_lock };
        return g_config.npcAutoDefeat;
    }

    bool NpcAutoDefeatIncludesRobots()
    {
        std::shared_lock lock{ g_lock };
        return g_config.npcAutoDefeatIncludeRobots;
    }

    bool IsFactionPacifyEnabled()
    {
        std::shared_lock lock{ g_lock };
        return g_config.factionPacify;
    }

    FormID CaptiveFactionID()
    {
        std::shared_lock lock{ g_lock };
        return g_config.captiveFaction;
    }

    float ParticipantScanRadius()
    {
        std::shared_lock lock{ g_lock };
        return g_config.participantScanRadius;
    }

    bool IsExplainEnabled()
    {
        std::shared_lock lock{ g_lock };
        return g_config.explainDecisions;
    }

    float WatchdogSeconds()
    {
        std::shared_lock lock{ g_lock };
        return g_config.watchdogSeconds;
    }

    std::filesystem::path RootPath()
    {
        std::shared_lock lock{ g_lock };
        return g_root.empty() ? ResolveRoot() : g_root;
    }
}