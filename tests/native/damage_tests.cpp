#include "Test.h"
#include "Core/Decision.h"
using namespace Test;
namespace {
    struct World {
        RE::PlayerCharacter player;
        RE::Actor enemy{1};
        World() { Add(player); Add(enemy); RE::PlayerCharacter::instance=&player; enemy.hostile.insert(&player); Decision::Initialize(); }
        void Drain() { RE::BSService::TaskQueue::GetSingleton()->Drain(); }
    };
}
TEST(lethal_health_clamps_and_queues_once_until_committed) {
    World w; w.player.health=20; Hit(w.player,-100,&w.enemy); CHECK(w.player.health==5);
    CHECK(!Defeat::IsDefeated(&w.player)); Hit(w.player,-100,&w.enemy); CHECK(w.player.health==5);
    CHECK(RE::BSService::TaskQueue::GetSingleton()->pending.size()==1); w.Drain();
    CHECK(Defeat::IsDefeated(&w.player) && downCalls==1); CHECK(Defeat::GetDefeatCause(&w.player)==DefeatCause::kHealth);
    CHECK(DefeatContext::GetLastPlayerAggressor()==&w.enemy); CHECK(Recovery::SecondsRemaining(&w.player)>0);
    Hit(w.player,-100,&w.enemy); CHECK(w.player.health==5); CHECK(downCalls==1);
}
TEST(weapon_context_snapshots_before_down_presentation_and_clears_on_load) {
    World w; RE::TESObjectWEAP weapon; weapon.id=42; Add(weapon);
    primaryWeaponID=weapon.id; onShow=[] {primaryWeaponID=0;};
    Hit(w.player,-200,&w.enemy); w.Drain();
    CHECK(primaryWeaponID==0); CHECK(DefeatContext::GetLastPlayerEquippedWeapon()==&weapon);
    DefeatContext::ClearPlayerContext(); CHECK(!DefeatContext::GetLastPlayerEquippedWeapon());
}
TEST(inline_queue_commits_only_after_original_health_write_returns) {
    World w; RE::BSService::TaskQueue::GetSingleton()->inlineTasks=true;
    bool checked=false; onOriginalDamage=[&] { checked=true; CHECK(!Defeat::IsDefeated(&w.player)); CHECK(w.player.health==5); };
    Hit(w.player,-100); CHECK(checked); CHECK(Defeat::IsDefeated(&w.player));
}
TEST(nonhealth_positive_and_non_damage_modifiers_pass_through) {
    World w; RE::ActorValueInfo other;
    Hit(w.player,-100,nullptr,RE::ACTOR_VALUE_MODIFIER::kDamage,&other); CHECK(w.player.health==0);
    w.player.health=100; Hit(w.player,-100,nullptr,RE::ACTOR_VALUE_MODIFIER::kTemporary); CHECK(w.player.health==0);
    Hit(w.player,20); CHECK(w.player.health==20); CHECK(RE::BSService::TaskQueue::GetSingleton()->pending.empty());
}
TEST(disabled_processing_resist_and_unavailable_queue_allow_vanilla_lethality) {
    World w; Eligibility::SetEnabled(false); Hit(w.player,-100); CHECK(w.player.health==0);
    Eligibility::SetEnabled(true); w.player.health=100; CHECK(PlayerDefeat::SetResistWindow(&w.player,60));
    Hit(w.player,-100); CHECK(w.player.health==0); CHECK(PlayerDefeat::SetResistWindow(&w.player,0));
    w.player.health=100; TestEngine::queueEnabled=0; Hit(w.player,-100); CHECK(w.player.health==0);
    CHECK(RE::BSService::TaskQueue::GetSingleton()->pending.empty());
}
TEST(accepted_hit_commits_even_when_settings_change_before_queue_drains) {
    World w; Hit(w.player,-100); Eligibility::SetEnabled(false); w.Drain(); CHECK(Defeat::IsDefeated(&w.player));
}
TEST(old_save_callbacks_cannot_defeat_player_or_clear_new_pending_latch) {
    World w; Hit(w.player,-100); auto* queue=RE::BSService::TaskQueue::GetSingleton();
    auto old=std::move(queue->pending.front()); queue->pending.clear();
    (void)RuntimeMutation::AdvanceEpoch(); PlayerDefeat::ResetTransientState(); w.player.health=100;
    Hit(w.player,-100); old(); CHECK(!Defeat::IsDefeated(&w.player));
    Hit(w.player,-100); CHECK(queue->pending.size()==1); w.Drain(); CHECK(downCalls==1);
}
TEST(null_aggressor_falls_back_to_combat_target_and_stages_context_before_quest) {
    World w; w.player.combatTarget=&w.enemy;
    Write("Consequences/Player/a.json",{{"version",3},{"id","held"},{"quest","Quest"},{"recoveryMode","held"},{"leaseTimeoutSeconds",10}});
    Consequence::Load(); Hit(w.player,-100); w.Drain();
    CHECK(DefeatContext::GetLastPlayerAggressor()==&w.enemy); CHECK(questContextAtStart=="held" && startedQuest=="Quest");
    CHECK(Recovery::GetLeaseHolder(&w.player)=="consequence:held"); CHECK(Recovery::SecondsRemaining(&w.player)==-1);
    CHECK(DefeatRegistry::Get(0x14)->leaseExpiresAtDay>calendar.day);
}
TEST(imminent_event_hysteresis_and_stale_epoch) {
    World w; auto receiver=std::make_shared<RE::BSScript::Object>(); Events::RegisterPlayerDefeatImminent(receiver,"imminent");
    w.player.health=30; Hit(w.player,-10); w.Drain(); CHECK(vm.calls.size()==1);
    Hit(w.player,-1); w.Drain(); CHECK(vm.calls.size()==1);
    w.player.health=50; Hit(w.player,-1); w.player.health=30; Hit(w.player,-10); w.Drain(); CHECK(vm.calls.size()==2);
    PlayerDefeat::ResetTransientState(); w.player.health=30; Hit(w.player,-10); (void)RuntimeMutation::AdvanceEpoch();
    w.Drain(); CHECK(vm.calls.size()==2);
}
TEST(resist_validation_clear_expiry_and_defeated_actor_refusal) {
    World w; CHECK(!PlayerDefeat::SetResistWindow(nullptr,1)); CHECK(!PlayerDefeat::SetResistWindow(&w.player,std::numeric_limits<float>::infinity()));
    CHECK(PlayerDefeat::SetResistWindow(&w.player,30)); CHECK(PlayerDefeat::IsResistWindowActive(&w.player));
    CHECK(PlayerDefeat::SetResistWindow(&w.player,-1)); CHECK(!PlayerDefeat::IsResistWindowActive(&w.player));
    CHECK(PlayerDefeat::SetResistWindow(&w.player,0.0001f)); CHECK(!PlayerDefeat::IsResistWindowActive(&w.player));
    CHECK(Defeat::DefeatActor(&w.player,nullptr)); CHECK(!PlayerDefeat::SetResistWindow(&w.player,10));
}
TEST(npc_auto_defeat_is_opt_in_and_filters_hostility_and_robots) {
    World w; Hit(w.enemy,-100); CHECK(w.enemy.health==0);
    Write("Settings/a.json",{{"version",3},{"npcAutoDefeat",true}}); Config::Load();
    w.enemy.health=100; w.enemy.hostile.clear(); Hit(w.enemy,-100); CHECK(w.enemy.health==0);
    w.enemy.hostile.insert(&w.player); w.enemy.health=100;
    RE::BGSKeyword robot; robot.id=3; Add(robot,"ActorTypeRobot"); Decision::Initialize(); w.enemy.keywords.insert(&robot);
    Hit(w.enemy,-100); CHECK(w.enemy.health==0);
    Write("Settings/a.json",{{"version",3},{"npcAutoDefeat",true},{"npcAutoDefeatIncludeRobots",true}}); Config::Reload();
    w.enemy.health=100; Hit(w.enemy,-100,&w.player); CHECK(w.enemy.health==5); w.Drain();
    CHECK(Defeat::GetDefeatCause(&w.enemy)==DefeatCause::kHealth); CHECK(Recovery::SecondsRemaining(&w.enemy)>0);
}
TEST(recent_attackers_exclude_nonhostile_dead_distant_and_nonactor_sources) {
    World w; RE::Actor friendActor(2), dead(3), distant(4); RE::TESObjectREFR hazard; hazard.id=5;
    Add(friendActor); Add(dead); Add(distant); Add(hazard); dead.dead=true; distant.hostile.insert(&w.player); distant.position.x=10000;
    Hit(w.player,-1,&friendActor); Hit(w.player,-1,&dead); Hit(w.player,-1,&distant); Hit(w.player,-1,&hazard);
    Hit(w.player,-1,&w.enemy); Hit(w.player,-100,&w.enemy); w.Drain();
    CHECK(DefeatContext::GetLastPlayerRecentHostileAttackers()==std::vector<RE::Actor*>({&w.enemy}));
}
TEST(outgoing_damage_breaks_truce_after_engine_write_and_obeys_load_epoch) {
    RE::Actor attacker(1), victim(2); Add(attacker); Add(victim);
    CHECK(Defeat::DefeatActor(&attacker,nullptr)); CHECK(Defeat::RecoverActor(&attacker,true,30));
    onOriginalDamage = [&]() { CHECK(Truce::IsActive(1)); };
    Hit(victim,-1,&attacker); CHECK(Truce::IsActive(1));
    RE::BSService::TaskQueue::GetSingleton()->Drain(); CHECK(!Truce::IsActive(1));
    CHECK(!Engine::HasCaptiveFactionApplied(attacker));
    CHECK(Defeat::DefeatActor(&attacker,nullptr)); CHECK(Defeat::RecoverActor(&attacker,true,30));
    Hit(victim,-1,&attacker); (void)RuntimeMutation::AdvanceEpoch();
    RE::BSService::TaskQueue::GetSingleton()->Drain(); CHECK(Truce::IsActive(1));
}
TEST(incoming_anonymous_healing_and_nonhealth_writes_preserve_player_truce) {
    World w; RE::ActorValueInfo other;
    CHECK(Truce::Begin(w.player,30));
    Hit(w.player,-1,&w.enemy); w.Drain(); CHECK(Truce::IsActive(w.player.id));
    Hit(w.enemy,-1); w.Drain(); CHECK(Truce::IsActive(w.player.id));
    Hit(w.enemy,1,&w.player); w.Drain(); CHECK(Truce::IsActive(w.player.id));
    Hit(w.enemy,-1,&w.player,RE::ACTOR_VALUE_MODIFIER::kDamage,&other); w.Drain(); CHECK(Truce::IsActive(w.player.id));
    Hit(w.enemy,-1,&w.player,RE::ACTOR_VALUE_MODIFIER::kTemporary); w.Drain(); CHECK(Truce::IsActive(w.player.id));
    CHECK(Engine::HasCaptiveFactionApplied(w.player));
}
TEST(attack_notifications_target_only_the_attacker_and_discard_old_save_callbacks) {
    World w; CHECK(Truce::Begin(w.player,30)); CHECK(Truce::Begin(w.enemy,30));
    Truce::QueueEndForAttack(w.enemy.id,RuntimeMutation::CurrentEpoch());
    CHECK(Truce::IsActive(w.enemy.id)); w.Drain();
    CHECK(!Truce::IsActive(w.enemy.id)); CHECK(!Engine::HasCaptiveFactionApplied(w.enemy));
    CHECK(Truce::IsActive(w.player.id)); CHECK(Engine::HasCaptiveFactionApplied(w.player));
    Truce::QueueEndForAttack(w.player.id,RuntimeMutation::CurrentEpoch());
    (void)RuntimeMutation::AdvanceEpoch(); w.Drain(); CHECK(Truce::IsActive(w.player.id));
    Truce::End(w.player);
    Truce::QueueEndForAttack(w.player.id,RuntimeMutation::CurrentEpoch());
    CHECK(Truce::Begin(w.player,30)); w.Drain(); CHECK(Truce::IsActive(w.player.id));
}
