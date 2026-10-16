#include "pch.h"

#include "Core/Config.h"

#include "REX/FModule.h"

#include <nlohmann/json.hpp>

namespace OSFDefeat::Config
{
    namespace
    {
        FrameworkSettings g_config;
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
            if (it == a_root.end()) {
                return;
            }
            if (!it->is_array()) {
                throw std::runtime_error(std::format("{} must be an array of numeric formIDs", a_key));
            }

            for (const auto& entry : *it) {
                if (const auto id = ParseFormID(entry)) {
                    a_out.insert(*id);
                } else {
                    throw std::runtime_error(std::format("{} entry is not a numeric formID", a_key));
                }
            }
        }

        void LoadFile(const std::filesystem::path& a_path, FrameworkSettings& a_config)
        {
            try {
                std::ifstream input{ a_path };
                if (!input) {
                    throw std::runtime_error("could not open file");
                }
                const auto json = nlohmann::json::parse(input, nullptr, true, true);
                const int version = json.value("version", 0);
                if (version != 1 && version != 2 && version != 3) {
                    REX::WARN("[OSFDefeat] settings JSON {} has unsupported version {}", a_path.string(), version);
                    return;
                }
                constexpr std::array<std::string_view, 18> kKnownKeys{
                    "version", "processingEnabled", "disableInScenes", "defaultAutoRecoveryDelaySeconds",
                    "defaultRecoverySeconds",
                    "npcAutoDefeat", "npcAutoDefeatIncludeRobots", "applyCaptiveFactionOnPlayerDefeat",
                    "factionPacify", "captiveFaction", "recentHostileAttackerRadius", "participantScanRadius",
                    "explainDecisions", "watchdogSeconds", "imminentHealthFraction",
                    "excludedLocations", "excludedQuests", "//"
                };
                for (const auto& [key, _] : json.items()) {
                    if (!key.starts_with("//") &&
                        std::find(kKnownKeys.begin(), kKnownKeys.end(), key) == kKnownKeys.end()) {
                        REX::WARN("[OSFDefeat] settings JSON {}: unknown field '{}' (typo?)", a_path.string(), key);
                    }
                }

                // A bad value must not leave half of this file applied. Parse into a
                // candidate and publish it only after the entire document succeeds.
                auto candidate = a_config;
                if (json.contains("processingEnabled")) {
                    candidate.processingEnabled = json.value("processingEnabled", true);
                }
                if (json.contains("disableInScenes")) {
                    candidate.disableInScenes = json.value("disableInScenes", false);
                }
                if (json.contains("defaultAutoRecoveryDelaySeconds") || json.contains("defaultRecoverySeconds")) {
                    const auto value = json.contains("defaultAutoRecoveryDelaySeconds")
                        ? json.value("defaultAutoRecoveryDelaySeconds", 8.0f)
                        : json.value("defaultRecoverySeconds", 8.0f);
                    if (!std::isfinite(value)) {
                        throw std::runtime_error("defaultAutoRecoveryDelaySeconds must be finite");
                    }
                    candidate.defaultAutoRecoveryDelaySeconds = std::max(1.0f, value);
                }
                if (json.contains("npcAutoDefeat")) {
                    candidate.npcAutoDefeat = json.value("npcAutoDefeat", false);
                }
                if (json.contains("npcAutoDefeatIncludeRobots")) {
                    candidate.npcAutoDefeatIncludeRobots = json.value("npcAutoDefeatIncludeRobots", false);
                }
                if (json.contains("applyCaptiveFactionOnPlayerDefeat")) {
                    candidate.applyCaptiveFactionOnPlayerDefeat = json.value("applyCaptiveFactionOnPlayerDefeat", false);
                } else if (json.contains("factionPacify")) {
                    candidate.applyCaptiveFactionOnPlayerDefeat = json.value("factionPacify", false);
                }
                if (const auto it = json.find("captiveFaction"); it != json.end()) {
                    if (const auto id = ParseFormID(*it)) {
                        candidate.captiveFaction = *id;
                    } else {
                        REX::WARN("[OSFDefeat] captiveFaction is not a numeric formID; keeping default");
                    }
                }
                if (json.contains("recentHostileAttackerRadius") || json.contains("participantScanRadius")) {
                    const auto value = json.contains("recentHostileAttackerRadius")
                        ? json.value("recentHostileAttackerRadius", 4096.0f)
                        : json.value("participantScanRadius", 4096.0f);
                    if (!std::isfinite(value)) {
                        throw std::runtime_error("recentHostileAttackerRadius must be finite");
                    }
                    candidate.recentHostileAttackerRadius = value;
                }
                if (json.contains("explainDecisions")) {
                    candidate.explainDecisions = json.value("explainDecisions", false);
                }
                if (json.contains("watchdogSeconds")) {
                    const auto value = json.value("watchdogSeconds", 600.0f);
                    if (!std::isfinite(value)) {
                        throw std::runtime_error("watchdogSeconds must be finite");
                    }
                    candidate.watchdogSeconds = value;
                }
                if (json.contains("imminentHealthFraction")) {
                    const auto value = json.value("imminentHealthFraction", 0.25f);
                    if (!std::isfinite(value)) {
                        throw std::runtime_error("imminentHealthFraction must be finite");
                    }
                    candidate.imminentHealthFraction = std::clamp(value, 0.0f, 0.9f);
                }
                ReadFormSet(json, "excludedLocations", candidate.excludedLocations);
                ReadFormSet(json, "excludedQuests", candidate.excludedQuests);
                if (!candidate.excludedLocations.empty()) {
                    REX::WARN("[OSFDefeat] excludedLocations is currently unsupported (location engine seam unproven); entries are parked safely");
                }
                if (!candidate.excludedQuests.empty()) {
                    REX::WARN("[OSFDefeat] excludedQuests is currently unsupported (quest-state engine seam unproven); entries are parked safely");
                }
                a_config = std::move(candidate);
                REX::INFO("[OSFDefeat] loaded framework settings {}", a_path.string());
            } catch (const std::exception& e) {
                REX::ERROR("[OSFDefeat] settings JSON {} failed: {}", a_path.string(), e.what());
            }
        }
    }

    void Load()
    {
        FrameworkSettings next;
        try {
            const auto& root = RootPath();
            const auto legacyRoot = root / "Validation";
            const auto settingsRoot = root / "Settings";
            if (std::filesystem::exists(legacyRoot)) {
                REX::INFO("[OSFDefeat] loading legacy settings directory {}; migrate edits to {} when convenient",
                    legacyRoot.string(), settingsRoot.string());
            }
            for (const auto& directory : { legacyRoot, settingsRoot }) {
                if (!std::filesystem::exists(directory)) {
                    continue;
                }
                std::vector<std::filesystem::path> files;
                for (const auto& entry : std::filesystem::directory_iterator{ directory }) {
                    if (entry.is_regular_file() && entry.path().extension() == ".json") {
                        files.push_back(entry.path());
                    }
                }
                std::ranges::sort(files);
                for (const auto& path : files) {
                    LoadFile(path, next);
                }
            }
            if (!std::filesystem::exists(legacyRoot) && !std::filesystem::exists(settingsRoot)) {
                REX::INFO("[OSFDefeat] no settings directory at {}; using defaults", settingsRoot.string());
            }
        } catch (const std::exception& e) {
            REX::ERROR("[OSFDefeat] settings directory read failed: {}", e.what());
        }

        std::unique_lock lock{ g_lock };
        g_config = std::move(next);
    }

    void Reload() { Load(); }

    FrameworkSettings Get()
    {
        std::shared_lock lock{ g_lock };
        return g_config;
    }

    bool IsProcessingEnabled()
    {
        std::shared_lock lock{ g_lock };
        return g_config.processingEnabled;
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

    bool ShouldApplyCaptiveFactionOnPlayerDefeat()
    {
        std::shared_lock lock{ g_lock };
        return g_config.applyCaptiveFactionOnPlayerDefeat;
    }

    FormID CaptiveFactionID()
    {
        std::shared_lock lock{ g_lock };
        return g_config.captiveFaction;
    }

    float RecentHostileAttackerRadius()
    {
        std::shared_lock lock{ g_lock };
        return g_config.recentHostileAttackerRadius;
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

    float ImminentHealthFraction()
    {
        std::shared_lock lock{ g_lock };
        return g_config.imminentHealthFraction;
    }

    const std::filesystem::path& RootPath()
    {
        static const auto root = std::filesystem::path{ REX::FModule::GetExecutingModule().GetFileName() }.parent_path()
            / "Data" / "SFSE" / "OSFDefeat";
        return root;
    }
}
