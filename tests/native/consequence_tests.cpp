#include "Test.h"
using namespace Test;
using namespace OSFDefeat::Consequence;
namespace {
    Json Choice(std::string id, int priority = 0) { return {{"version",3},{"id",id},{"displayName",id},{"priority",priority}}; }
    void Put(std::string file, Json json) { Write("Consequences/Player/" + file + ".json", json); }
    auto Select(RE::Actor* victim = nullptr) { return SelectPlayerConsequence(victim,nullptr,DefeatCause::kHealth); }
}
TEST(fallback_uses_configured_recovery) {
    Write("Settings/a.json", {{"version",3},{"defaultAutoRecoveryDelaySeconds",23}}); Config::Load();
    auto c = Select(); CHECK(c.id == "osfdefeat.builtin.down_then_recover"); CHECK(c.autoRecoveryDelaySeconds == 23); CHECK(!c.HoldsRecovery());
}
TEST(legacy_and_canonical_schema_normalization) {
    Put("a", {{"version",1},{"name","legacy"},{"holdRecovery",true},{"maxHoldSeconds",90},{"recoverySeconds",12},{"weight",0}}); Load();
    auto c = Select(); CHECK(c.id == "legacy" && c.displayName == "legacy" && c.HoldsRecovery());
    CHECK(c.leaseTimeoutSeconds == 90 && c.autoRecoveryDelaySeconds == 12 && c.weight == 1);
    auto modern = Choice("modern"); modern.update({{"holdRecovery",true},{"recoveryMode","automatic"},{"recoverySeconds",30},
        {"autoRecoveryDelaySeconds",-1},{"maxHoldSeconds",90},{"leaseTimeoutSeconds",20},{"startQuest",false},{"quest","Q"}});
    Put("a", modern); Reload(); c = Select(); CHECK(c.id == "modern" && !c.HoldsRecovery());
    CHECK(c.autoRecoveryDelaySeconds == 1 && c.leaseTimeoutSeconds == 20 && !c.startQuest && c.quest == "Q");
}
TEST(malformed_and_duplicate_files_do_not_replace_first_valid_choice) {
    Put("a", Choice("same",4)); Put("z", Choice("same",99));
    Raw("Consequences/Player/b.json", "{bad"); Put("c",{{"version",99},{"id","future"},{"priority",100}});
    Put("d",{{"version",3},{"id",""},{"priority",100}}); auto invalid=Choice("invalid",100); invalid["recoveryMode"]="bad"; Put("e",invalid);
    Load(); auto c=Select(); CHECK(c.id=="same" && c.priority==4); CHECK(Logged("duplicate consequence ID")); CHECK(Logged("failed"));
}
TEST(filters_run_before_priority_and_negative_priorities_compete) {
    Put("a",Choice("eligible",-10));
    auto c=Choice("disabled",100); c["enabled"]=false; Put("b",c);
    c=Choice("ship",100); c["domain"]="ship"; Put("c",c);
    c=Choice("future",100); c["domain"]="unknown"; Put("d",c);
    c=Choice("condition",100); c["conditions"]={{"cause","scripted"}}; Put("e",c);
    c=Choice("invalid",100); c["conditions"]={{"typo",true}}; Put("f",c);
    Load(); CHECK(Select().id=="eligible");
}
TEST(highest_priority_pool_preserves_authored_order_and_64_bit_weight_sum) {
    Write("Settings/a.json", {{"version",3},{"explainDecisions",true}}); Config::Load();
    Put("a",Choice("low",1)); auto a=Choice("first",5); a["weight"]=2147483647; Put("b",a);
    auto b=Choice("second",5); b["weight"]=2147483647; Put("c",b); Put("d",Choice("lower",2)); Load();
    for(int i=0;i<64;++i) {
        auto c=Select(); CHECK(c.id=="first" || c.id=="second");
        // Validate each actual roll's interval, without a flaky distribution threshold.
        const auto& log=TestEngine::logs.back();
        const auto roll=std::stoll(log.substr(log.find("roll ")+5));
        CHECK(c.id==(roll<=2147483647LL ? "first" : "second"));
    }
    CHECK(Logged("['first'(w2147483647), 'second'(w2147483647)]")); CHECK(Logged("/4294967294"));
}
TEST(overrides_apply_by_stable_id_and_v1_display_name) {
    auto a=Choice("a",1); a["displayName"]="Visible"; Put("a",a); Put("b",Choice("b",2));
    Write("Overrides.json",{{"version",2},{"consequences",{{"a",{{"priority",5},{"weight",0}}},{"missing",{{"enabled",false}}}}}});
    Load(); CHECK(Select().id=="a" && Select().weight==1); CHECK(Logged("unknown consequence 'missing'"));
    Write("Overrides.json",{{"version",1},{"consequences",{{"Visible",{{"enabled",false}}}}}}); Reload(); CHECK(Select().id=="b");
}
TEST(cooldown_starts_on_fire_survives_reload_and_resets_on_revert) {
    auto a=Choice("cool",5); a["cooldownSeconds"]=60; Put("a",a); Put("b",Choice("other")); Load();
    auto c=Select(); CHECK(c.id=="cool"); CHECK(Select().id=="cool"); MarkFired(c,nullptr);
    CHECK(Select().id=="other"); Reload(); CHECK(Select().id=="other");
    ResetRuntimeState(); CHECK(Select().id=="cool");
}
TEST(once_per_cell_and_unknown_cell_behavior) {
    auto a=Choice("once",5); a["oncePerLocation"]=true; Put("a",a); Put("b",Choice("other")); Load();
    RE::Actor victim(1); RE::TESForm cell; cell.id=100; victim.parentCell=&cell;
    MarkFired(Select(&victim),&victim); CHECK(Select(&victim).id=="other");
    cell.id=101; CHECK(Select(&victim).id=="once"); victim.parentCell=nullptr;
    MarkFired(Select(&victim),&victim); CHECK(Select(&victim).id=="once");
}
