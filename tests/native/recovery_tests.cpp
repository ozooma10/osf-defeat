#include "Test.h"
using namespace Test;

TEST(timer_validation_and_legacy_hold_semantics) {
    RE::Actor actor(1); Add(actor);
    CHECK(!Recovery::ScheduleAutomatic(&actor,5)); CHECK(!Recovery::Hold(nullptr)); CHECK(Recovery::SecondsRemaining(&actor)==0);
    CHECK(Defeat::DefeatActor(&actor,nullptr));
    for(float invalid : {0.0f,-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) CHECK(!Recovery::ScheduleAutomatic(&actor,invalid));
    CHECK(Recovery::SetTimer(&actor,0)); CHECK(Recovery::SecondsRemaining(&actor)==-1);
    CHECK(Recovery::SetTimer(&actor,-1)); CHECK(!Recovery::SetTimer(&actor,std::numeric_limits<float>::quiet_NaN()));
    CHECK(Recovery::SetTimer(&actor,5)); CHECK(Near(Recovery::SecondsRemaining(&actor),5));
}
TEST(deadline_uses_game_time_scale_and_minimum_one_second) {
    calendar.day=2; calendar.scale=40;
    CHECK(Near((Recovery::DeadlineForSeconds(10)-2)*86400/40,10));
    CHECK(Near((Recovery::DeadlineForSeconds(0.1f)-2)*86400/40,1));
    calendar.scale=0; CHECK(Near((Recovery::DeadlineForSeconds(10)-2)*86400,10));
    RE::Calendar::instance=nullptr; CHECK(Near(Recovery::DeadlineForSeconds(10)*86400/20,10));
}
TEST(lease_ownership_extension_release_and_schedule_override) {
    RE::Actor actor(1); CHECK(Defeat::DefeatActor(&actor,nullptr));
    CHECK(!Recovery::AcquireLease(&actor,"",10)); CHECK(!Recovery::AcquireLease(nullptr,"a",10));
    CHECK(!Recovery::AcquireLease(&actor,"a",std::numeric_limits<float>::infinity()));
    CHECK(Recovery::AcquireLease(&actor,"a",10)); const float first=DefeatRegistry::Get(1)->leaseExpiresAtDay;
    CHECK(!Recovery::AcquireLease(&actor,"b",20)); CHECK(!Recovery::ReleaseLease(&actor,"b"));
    CHECK(Recovery::GetLeaseHolder(&actor)=="a"); CHECK(DefeatRegistry::Get(1)->leaseExpiresAtDay==first);
    CHECK(Recovery::AcquireLease(&actor,"a",20)); CHECK(DefeatRegistry::Get(1)->leaseExpiresAtDay>first);
    CHECK(Recovery::ReleaseLease(&actor,"a")); CHECK(Recovery::GetLeaseHolder(&actor).empty());
    CHECK(Near(Recovery::SecondsRemaining(&actor),8)); CHECK(!Recovery::ReleaseLease(&actor,"a"));
    CHECK(Recovery::AcquireLease(&actor,"a",10)); CHECK(Recovery::ScheduleAutomatic(&actor,5));
    CHECK(Recovery::GetLeaseHolder(&actor).empty() && DefeatRegistry::Get(1)->leaseExpiresAtDay==0);
}
TEST(automatic_recovery_fires_at_deadline_and_leaves_held_npc_alone) {
    RE::Actor automatic(1), held(2); Add(automatic); Add(held);
    CHECK(Defeat::DefeatActor(&automatic,nullptr)); CHECK(Defeat::DefeatActor(&held,nullptr));
    CHECK(Recovery::ScheduleAutomatic(&automatic,10)); const float deadline=DefeatRegistry::Get(1)->autoRecoverAtDay;
    Recovery::Tick(); CHECK(Defeat::IsDefeated(&automatic)); CHECK(DefeatRegistry::Get(2)->leaseExpiresAtDay==0);
    calendar.day=deadline; Recovery::Tick(); CHECK(!Defeat::IsDefeated(&automatic)); CHECK(Defeat::IsDefeated(&held));
}
TEST(player_hold_auto_arms_watchdog_and_reasserts_presentation) {
    RE::PlayerCharacter player; Add(player); RE::PlayerCharacter::instance=&player;
    CHECK(Defeat::DefeatActor(&player,nullptr)); Recovery::Tick();
    auto data=DefeatRegistry::Get(0x14); CHECK(data->leaseExpiresAtDay>calendar.day); CHECK(!data->CopyLeaseHolderId().empty());
    CHECK(reassertCalls==1); calendar.day=data->leaseExpiresAtDay; Recovery::Tick();
    CHECK(!Defeat::IsDefeated(&player)); CHECK(clearCalls==1); CHECK(Logged("watchdog:"));
}
TEST(leased_npc_watchdog_clears_capture_and_watchdog_can_be_disabled) {
    RE::Actor actor(1); Add(actor); CHECK(Capture::CaptureActor(&actor,nullptr)); CHECK(Recovery::AcquireLease(&actor,"content",5));
    calendar.day=DefeatRegistry::Get(1)->leaseExpiresAtDay; Recovery::Tick();
    CHECK(!Defeat::IsDefeated(&actor)); CHECK(!Capture::IsCaptive(&actor)); CHECK(!Engine::HasCaptiveFactionApplied(actor));
    Write("Settings/a.json",{{"version",3},{"watchdogSeconds",0}}); Config::Load();
    RE::PlayerCharacter player; Add(player); CHECK(Defeat::DefeatActor(&player,nullptr));
    CHECK(Recovery::AcquireLease(&player,"content",0)); Recovery::Tick(); CHECK(DefeatRegistry::Get(0x14)->leaseExpiresAtDay==0);
    CHECK(Recovery::AcquireLease(&player,"content",5)); CHECK(DefeatRegistry::Get(0x14)->leaseExpiresAtDay>calendar.day);
}
TEST(unresolved_automatic_recovery_retries_when_actor_returns) {
    RE::Actor actor(1); CHECK(Defeat::DefeatActor(&actor,nullptr)); CHECK(Recovery::ScheduleAutomatic(&actor,2));
    calendar.day=DefeatRegistry::Get(1)->autoRecoverAtDay; Recovery::Tick(); CHECK(DefeatRegistry::Contains(1));
    Add(actor); Recovery::Tick(); CHECK(!DefeatRegistry::Contains(1));
}
TEST(truce_recovery_preserves_pacification_until_deadline) {
    RE::Actor actor(1); Add(actor); CHECK(Capture::CaptureActor(&actor,nullptr));
    onRestore = [&](RE::Actor& a) { CHECK(Truce::IsActive(a.id)); CHECK(Engine::HasCaptiveFactionApplied(a)); };
    CHECK(Defeat::RecoverActor(&actor,true,30)); CHECK(!Defeat::IsDefeated(&actor));
    CHECK(!Capture::IsCaptive(&actor)); CHECK(Truce::IsActive(1));
    const auto deadline=Truce::Snapshot()[0].expiresAtDay;
    calendar.day=deadline-0.00001f; Recovery::Tick(); CHECK(Truce::IsActive(1));
    calendar.day=deadline; Recovery::Tick(); CHECK(!Truce::IsActive(1));
    CHECK(!Engine::HasCaptiveFactionApplied(actor)); CHECK(!Truce::End(actor));
}
TEST(truce_invalid_or_unavailable_recovery_keeps_defeat_owned) {
    RE::Actor actor(1); Add(actor); CHECK(Defeat::DefeatActor(&actor,nullptr));
    for(float value : {-1.0f,3601.0f,std::numeric_limits<float>::quiet_NaN()}) CHECK(!Defeat::RecoverActor(&actor,true,value));
    factionAvailable=false; CHECK(!Defeat::RecoverActor(&actor,true,30));
    factionAvailable=true; factionApplyWorks=false; CHECK(!Defeat::RecoverActor(&actor,true,30));
    CHECK(Defeat::IsDefeated(&actor)); CHECK(restoreCalls==0); CHECK(Truce::Snapshot().empty());
}
TEST(truce_preserves_unowned_membership_and_cleans_up_on_new_defeat) {
    RE::Actor actor(1); Add(actor); preexistingFactions.insert(1);
    CHECK(Defeat::DefeatActor(&actor,nullptr)); CHECK(Defeat::RecoverActor(&actor,true,30));
    CHECK(!Truce::Snapshot()[0].ownsFaction); CHECK(Truce::End(actor)); CHECK(Engine::IsInCaptiveFaction(actor));
    preexistingFactions.clear(); CHECK(Defeat::DefeatActor(&actor,nullptr)); CHECK(Defeat::RecoverActor(&actor,true,30));
    CHECK(Defeat::DefeatActor(&actor,nullptr)); CHECK(!Truce::IsActive(1)); CHECK(!Engine::HasCaptiveFactionApplied(actor));
}
TEST(truce_emergency_cleanup_retries_unresolved_actor_and_healthy_cleanup) {
    RE::Actor actor(1); CHECK(Defeat::DefeatActor(&actor,nullptr)); CHECK(Defeat::RecoverActor(&actor,true,30));
    CHECK(Defeat::ForceRecoverAll()==0); CHECK(Truce::IsActive(1)); CHECK(Truce::Snapshot()[0].expiresAtDay==0);
    Add(actor); Recovery::Tick(); CHECK(!Truce::IsActive(1)); CHECK(!Engine::HasCaptiveFactionApplied(actor));
    CHECK(Defeat::DefeatActor(&actor,nullptr)); CHECK(Defeat::RecoverActor(&actor,true,30));
    Defeat::ForceRecoverAll(); CHECK(Truce::Snapshot().empty()); CHECK(Engine::SnapshotCaptiveFactions().empty());
}
