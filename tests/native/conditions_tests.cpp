#include "Test.h"
#include "Core/FormToken.h"
#include "Core/Decision.h"
using namespace Test;
using namespace OSFDefeat::Conditions;

TEST(absent_gates_pass_but_authored_empty_actor_or_cause_fails_closed) {
    CHECK(Explain(Parse(Json::object(), "test"), nullptr, nullptr) == EvalResult::kPass);
    for (const auto& value : std::vector<Json>{nullptr, 1, "bad", Json::array(), {{"aggressor",Json::object()}},
        {{"victim",false}}, {{"cause",Json::array()}}, {{"cause","em"}}, {{"cause",Json::array({"health",42})}}}) {
        CHECK(Explain(Parse(value,"test"), nullptr, nullptr) == EvalResult::kInvalid);
    }
}
TEST(unknown_reserved_and_malformed_conditions_are_parked) {
    for (const auto& key : {"locations", "questsRunning", "questsCompleted", "typo"}) {
        CHECK(!Parse(Json{{key, Json::array()}}, "test").valid);
    }
    for (const auto& block : std::vector<Json>{{{"keywords","bad"}}, {{"keywords",Json::array({"",42})}},
        {{"keywords",Json::array({"K"})},{"match","either"}}, {{"keywords",Json::array({"K"})},{"typo",true}}}) {
        CHECK(!Parse({{"aggressor",block}}, "test").valid);
    }
}
TEST(cause_gate_and_explanation_order) {
    auto c = Parse({{"cause",Json::array({"health","scripted"})}, {"aggressor",{{"keywords",Json::array({"Missing"})}}}},"test");
    CHECK(c.valid); CHECK(Explain(c,nullptr,nullptr,DefeatCause::kNone) == EvalResult::kCauseFailed);
    CHECK(Explain(c,nullptr,nullptr,DefeatCause::kHealth) == EvalResult::kAggressorFailed);
}
TEST(actor_any_all_defaults_and_unresolved_tokens) {
    RE::BGSKeyword keyword; keyword.id = 10; Add(keyword,"K");
    RE::TESFaction faction; faction.id = 11; Add(faction,"F");
    RE::Actor victim(1), aggressor(2); aggressor.keywords.insert(&keyword); victim.keywords.insert(&keyword);
    const Json block{{"keywords",Json::array({"K"})},{"factions",Json::array({"F"})}};
    auto c = Parse({{"aggressor",block},{"victim",block}},"test");
    CHECK(!c.aggressor.matchAll && c.victim.matchAll);
    CHECK(Explain(c,&victim,&aggressor) == EvalResult::kVictimFailed);
    victim.factions[&faction] = 0; CHECK(Explain(c,&victim,&aggressor) == EvalResult::kPass);
    c.aggressor.matchAll = true; CHECK(Explain(c,&victim,&aggressor) == EvalResult::kAggressorFailed);
    aggressor.factions[&faction] = 0; CHECK(Explain(c,&victim,&aggressor) == EvalResult::kPass);
    c.aggressor.keywords.push_back("Missing"); CHECK(Explain(c,&victim,&aggressor) == EvalResult::kAggressorFailed);
}
TEST(form_resolution_editor_first_hex_strict_and_type_safe) {
    RE::BGSKeyword keyword; keyword.id = 0xAB; Add(keyword, "K");
    RE::TESFaction faction; faction.id = 0xCD; Add(faction,"AB");
    CHECK(ResolveFormToken<RE::BGSKeyword>("K") == &keyword);
    CHECK(ResolveFormToken<RE::BGSKeyword>("0xAB") == &keyword);
    CHECK(ResolveFormToken<RE::TESFaction>("AB") == &faction);
    CHECK(ResolveFormToken<RE::BGSKeyword>("AB") == &keyword);
    for (const auto token : {"", "0x", "123456789", " AB", "ABtail", "-1", "0xCD"}) CHECK(!ResolveFormToken<RE::BGSKeyword>(token));
}
TEST(classification_turret_precedes_robot_and_hostility_requires_both_actors) {
    RE::BGSKeyword robot, turret; robot.id=1; turret.id=2; Add(robot,"ActorTypeRobot"); Add(turret,"ActorTypeTurret");
    Decision::Initialize(); RE::Actor actor(3), target(4);
    CHECK(Decision::Classify(nullptr) == Decision::ActorClass::Flesh);
    actor.keywords.insert(&robot); CHECK(Decision::Classify(&actor) == Decision::ActorClass::Robot);
    actor.keywords.insert(&turret); CHECK(Decision::Classify(&actor) == Decision::ActorClass::Turret);
    CHECK(Decision::IsNonFlesh(&actor)); CHECK(!Decision::IsHostile(&actor,&target));
    actor.hostile.insert(&target); CHECK(Decision::IsHostile(&actor,&target)); CHECK(!Decision::IsHostile(nullptr,&target));
    RE::TESForm::editors.clear(); Decision::Initialize(); CHECK(Decision::Classify(&actor) == Decision::ActorClass::Flesh);
}
