#include "Test.h"
using namespace Test;

TEST(defaults_without_files) {
    CHECK(Config::IsProcessingEnabled()); CHECK(!Config::ShouldDisableInScenes());
    CHECK(Config::DefaultAutoRecoveryDelaySeconds() == 8); CHECK(!Config::IsNpcAutoDefeatEnabled());
    CHECK(!Config::NpcAutoDefeatIncludesRobots()); CHECK(!Config::ShouldApplyCaptiveFactionOnPlayerDefeat());
    CHECK(Config::CaptiveFactionID() == 0x3E0C8); CHECK(Config::RecentHostileAttackerRadius() == 4096);
    CHECK(Config::WatchdogSeconds() == 600); CHECK(Config::ImminentHealthFraction() == 0.25f);
}
TEST(legacy_directory_then_sorted_modern_overrides) {
    Write("Validation/z.json", {{"version", 1}, {"defaultRecoverySeconds", 12}, {"factionPacify", true}, {"participantScanRadius", 80}});
    Write("Settings/z.json", {{"version", 3}, {"defaultAutoRecoveryDelaySeconds", 15}});
    Write("Settings/a.json", {{"version", 2}, {"defaultRecoverySeconds", 10}, {"npcAutoDefeat", true}});
    Config::Load(); CHECK(Config::DefaultAutoRecoveryDelaySeconds() == 15);
    CHECK(Config::ShouldApplyCaptiveFactionOnPlayerDefeat()); CHECK(Config::RecentHostileAttackerRadius() == 80);
    CHECK(Config::IsNpcAutoDefeatEnabled());
}
TEST(canonical_keys_win_and_values_are_clamped) {
    Write("Settings/a.json", {{"version", 3}, {"defaultAutoRecoveryDelaySeconds", -5}, {"defaultRecoverySeconds", 100},
        {"applyCaptiveFactionOnPlayerDefeat", false}, {"factionPacify", true}, {"recentHostileAttackerRadius", -1},
        {"participantScanRadius", 100}, {"imminentHealthFraction", 5}, {"watchdogSeconds", 0}});
    Config::Load(); CHECK(Config::DefaultAutoRecoveryDelaySeconds() == 1);
    CHECK(!Config::ShouldApplyCaptiveFactionOnPlayerDefeat()); CHECK(Config::RecentHostileAttackerRadius() == -1);
    CHECK(Config::ImminentHealthFraction() == 0.9f); CHECK(Config::WatchdogSeconds() == 0);
}
TEST(invalid_file_is_atomic_and_later_files_still_load) {
    Write("Settings/a.json", {{"version", 3}, {"defaultAutoRecoveryDelaySeconds", 17}});
    Write("Settings/b.json", {{"version", 3}, {"processingEnabled", false}, {"defaultAutoRecoveryDelaySeconds", 90}, {"excludedQuests", "bad"}});
    Write("Settings/c.json", {{"version", 3}, {"explainDecisions", true}});
    Config::Load(); CHECK(Config::IsProcessingEnabled()); CHECK(Config::DefaultAutoRecoveryDelaySeconds() == 17);
    CHECK(Config::IsExplainEnabled()); CHECK(Logged("failed"));
}
TEST(malformed_and_future_versions_keep_defaults) {
    Raw("Settings/a.json", "{broken"); Write("Settings/b.json", {{"version", 99}, {"processingEnabled", false}});
    Config::Load(); CHECK(Config::IsProcessingEnabled()); CHECK(Logged("unsupported version")); CHECK(Logged("failed"));
}
TEST(reload_starts_from_defaults_and_ignores_non_json_files) {
    Write("Settings/a.json", {{"version", 3}, {"processingEnabled", false}}); Config::Load(); CHECK(!Config::IsProcessingEnabled());
    std::filesystem::remove(Config::RootPath() / "Settings/a.json");
    Raw("Settings/ignored.txt", "{bad"); Config::Reload(); CHECK(Config::IsProcessingEnabled());
}
TEST(comments_and_numeric_form_tokens) {
    Raw("Settings/a.json", R"({ // authored comment
      "version":3, "captiveFaction":"0X1234", "excludedLocations":[20,"0x20","ABC"], "//note":"ok", "typo":1
    })");
    Config::Load(); CHECK(Config::CaptiveFactionID() == 0x1234); CHECK(Logged("parked safely")); CHECK(Logged("unknown field 'typo'"));
    CHECK(!Logged("unknown field '//note'"));
}
TEST(nonfinite_conversion_and_invalid_form_set_reject_entire_file) {
    Raw("Settings/a.json", R"({"version":3,"processingEnabled":false,"watchdogSeconds":1e100})");
    Config::Load(); CHECK(Config::IsProcessingEnabled()); CHECK(Config::WatchdogSeconds() == 600);
    Raw("Settings/a.json", R"({"version":3,"processingEnabled":false,"excludedLocations":[-1]})");
    Config::Load(); CHECK(Config::IsProcessingEnabled());
}
TEST(eligibility_precedence_and_scene_gate) {
    RE::Actor actor(50); CHECK(Eligibility::IsEligible(&actor)); CHECK(Eligibility::Explain(nullptr) == Eligibility::Verdict::kNoVictim);
    actor.dead = true; CHECK(Eligibility::Explain(&actor) == Eligibility::Verdict::kVictimDead);
    actor.dead = false; actor.inScene = true; CHECK(Eligibility::IsEligible(&actor));
    Write("Settings/a.json", {{"version",3},{"disableInScenes",true}}); Config::Reload();
    CHECK(Eligibility::Explain(&actor) == Eligibility::Verdict::kInScene);
    Write("Settings/a.json", {{"version",3},{"processingEnabled",false}}); Config::Reload();
    CHECK(Eligibility::Explain(nullptr) == Eligibility::Verdict::kConfigDisabled);
    Eligibility::SetEnabled(false); CHECK(Eligibility::Explain(nullptr) == Eligibility::Verdict::kApiDisabled);
}
