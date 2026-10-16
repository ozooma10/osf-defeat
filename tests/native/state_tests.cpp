#include "Test.h"
using namespace Test;

TEST(defeat_rejects_null_dead_and_duplicate_without_side_effects) {
    RE::Actor victim(1), aggressor(2); Add(victim); Add(aggressor);
    CHECK(!Defeat::DefeatActor(nullptr,nullptr)); victim.dead=true; CHECK(!Defeat::DefeatActor(&victim,nullptr));
    victim.dead=false; CHECK(Defeat::DefeatActor(&victim,&aggressor,DefeatCause::kHealth));
    CHECK(!Defeat::DefeatActor(&victim,nullptr)); CHECK(downCalls==1);
    CHECK(Defeat::GetDefeatCause(&victim)==DefeatCause::kHealth); CHECK(DefeatRegistry::Get(1)->aggressor==2);
    CHECK(Defeat::IsDamageImmune(&victim)); CHECK(!Defeat::IsDamageImmune(nullptr));
}
TEST(recovery_is_idempotent_under_engine_reentry_and_clears_capture) {
    RE::Actor actor(1); Add(actor); CHECK(Capture::CaptureActor(&actor,nullptr));
    bool nestedResult=true; onRestore=[&](RE::Actor& value) { nestedResult=Defeat::RecoverActor(&value,true); };
    CHECK(Defeat::RecoverActor(&actor,true)); CHECK(!nestedResult); CHECK(restoreCalls==1);
    CHECK(!Capture::IsCaptive(&actor)); CHECK(!Engine::HasCaptiveFactionApplied(actor)); CHECK(!Defeat::RecoverActor(&actor,true));
}
TEST(player_presentation_and_callbacks_happen_once_per_transition) {
    RE::PlayerCharacter player; Add(player); RE::PlayerCharacter::instance=&player;
    auto receiver=std::make_shared<RE::BSScript::Object>();
    CHECK(Events::RegisterPlayerDefeated(receiver,"down")!=0); CHECK(Events::RegisterPlayerRecovered(receiver,"up")!=0);
    CHECK(Defeat::DefeatActor(&player,nullptr)); CHECK(!Defeat::DefeatActor(&player,nullptr));
    CHECK(showCalls==1 && vm.calls.size()==1 && vm.calls[0].name=="down");
    CHECK(Defeat::RecoverActor(&player,true)); CHECK(clearCalls==1 && vm.calls.size()==2 && vm.calls[1].name=="up");
}
TEST(ai_suspension_preserves_prior_disabled_state_and_player_ai) {
    RE::Actor actor(1); actor.boolBits.enabled=false;
    Defeat::SuspendAI(&actor); Defeat::SuspendAI(&actor); CHECK(Defeat::IsAISuspended(&actor));
    Defeat::ResumeAI(&actor); CHECK(!actor.boolBits.enabled); CHECK(!Defeat::IsAISuspended(&actor));
    actor.boolBits.enabled=true; Defeat::SuspendAI(&actor); CHECK(!actor.boolBits.enabled);
    Defeat::ResumeAI(&actor); CHECK(actor.boolBits.enabled);
    RE::PlayerCharacter player; Defeat::SuspendAI(&player); CHECK(player.boolBits.enabled);
    Defeat::ResumeAI(&player); CHECK(player.boolBits.enabled);
}
TEST(loaded_filter_and_restore_do_not_replay_events_or_overwrite_snapshot) {
    RE::Actor actor(1), unloaded(2); Add(actor); Add(unloaded); unloaded.loadedData.value=nullptr;
    ActorStateSnapshot prior; prior.valid=true; prior.ghost=true; prior.aiEnabled=false;
    Defeat::RestoreFromSave(1,5,9,6,7,"owner",DefeatCause::kHealth,prior);
    Defeat::RestoreFromSave(1,20,0,0,0,"wrong",DefeatCause::kNone,{});
    CHECK(DefeatRegistry::Get(1)->priorState.ghost && DefeatRegistry::Get(1)->registeredDay==5);
    CHECK(DefeatRegistry::Get(1)->CopyLeaseHolderId()=="owner"); CHECK(downCalls==1 && vm.calls.empty());
    CHECK(Defeat::DefeatActor(&unloaded,nullptr)); DefeatRegistry::Add(99,1,0);
    CHECK(Defeat::GetAllDefeated(false).size()==2); CHECK(Defeat::GetAllDefeated(true)==std::vector<RE::Actor*>({&actor}));
}
TEST(capture_holds_and_recapture_preserves_faction_ownership) {
    RE::Actor captive(1), captor(2), next(3); Add(captive); Add(captor); Add(next);
    CHECK(Capture::CaptureActor(&captive,&captor)); CHECK(Capture::GetCaptorID(&captive)==2);
    CHECK(Recovery::SecondsRemaining(&captive)==-1); CHECK(Defeat::GetDefeatCause(&captive)==DefeatCause::kScripted);
    CHECK(Capture::CaptureActor(&captive,&next)); CHECK(Capture::GetCaptives(&captor).empty());
    CHECK(Capture::GetCaptives(&next)==std::vector<RE::Actor*>({&captive})); CHECK(Capture::Snapshot()[0].removeOwnedFactionOnRelease);
    CHECK(Capture::ReleaseCaptive(&captive,false)); CHECK(Defeat::IsDefeated(&captive)); CHECK(!Engine::HasCaptiveFactionApplied(captive));
    CHECK(!Capture::ReleaseCaptive(&captive,false));
}
TEST(capture_preserves_membership_it_did_not_introduce) {
    RE::Actor captive(1); Add(captive); preexistingFactions.insert(1);
    CHECK(Capture::CaptureActor(&captive,nullptr)); CHECK(!Capture::Snapshot()[0].removeOwnedFactionOnRelease);
    CHECK(Capture::ReleaseCaptive(&captive,false)); CHECK(Engine::IsInCaptiveFaction(captive));
    preexistingFactions.clear(); Engine::ApplyCaptiveFaction(captive);
    CHECK(Capture::CaptureActor(&captive,nullptr)); CHECK(!Capture::Snapshot()[0].removeOwnedFactionOnRelease);
    CHECK(Capture::ReleaseCaptive(&captive,false)); CHECK(Engine::HasCaptiveFactionApplied(captive));
}
TEST(failed_capture_rolls_back_new_defeat_and_existing_timers) {
    RE::Actor actor(1); Add(actor); factionAvailable=false;
    CHECK(!Capture::CaptureActor(&actor,nullptr)); CHECK(downCalls==0);
    factionAvailable=true; factionApplyWorks=false;
    CHECK(!Capture::CaptureActor(&actor,nullptr)); CHECK(!Defeat::IsDefeated(&actor)); CHECK(!Capture::IsCaptive(&actor));
    CHECK(Defeat::DefeatActor(&actor,nullptr)); CHECK(Recovery::AcquireLease(&actor,"prior",20));
    auto data=DefeatRegistry::Get(1); const float deadline=data->leaseExpiresAtDay;
    CHECK(!Capture::CaptureActor(&actor,nullptr)); CHECK(Defeat::IsDefeated(&actor));
    CHECK(data->leaseExpiresAtDay==deadline && data->CopyLeaseHolderId()=="prior");
    CHECK(Recovery::ScheduleAutomatic(&actor,30)); const float automatic=data->autoRecoverAtDay;
    CHECK(!Capture::CaptureActor(&actor,nullptr)); CHECK(data->autoRecoverAtDay==automatic);
}
TEST(release_preserves_record_if_recovery_cannot_commit) {
    RE::Actor actor(1); Capture::RestoreCapture(1,2,1,true);
    CHECK(!Capture::ReleaseCaptive(&actor,true)); CHECK(Capture::IsCaptive(&actor));
}
TEST(transfer_validates_vm_and_packs_player_target_at_dispatch_time) {
    RE::Actor actor(1); RE::PlayerCharacter player; Add(actor); RE::PlayerCharacter::instance=&player;
    CHECK(!Capture::TransferCaptive(&actor,&player)); CHECK(Capture::CaptureActor(&actor,nullptr));
    gameVM.vm=nullptr; CHECK(!Capture::TransferCaptive(&actor,&player)); gameVM.vm=&vm;
    vm.handles.available=false; CHECK(!Capture::TransferCaptive(&actor,&player)); vm.handles.available=true;
    vm.accept=false; CHECK(!Capture::TransferCaptive(&actor,&player)); vm.accept=true;
    CHECK(Capture::TransferCaptive(&actor,&player)); CHECK(vm.calls.back().name=="ObjectReference.MoveTo");
    RE::BSScrapArray<RE::BSScript::Variable> args; CHECK(vm.calls.back().args(args)); CHECK(args.size()==6);
    CHECK(std::any_cast<RE::TESObjectREFR*>(args[0].value)==&player); CHECK(std::any_cast<bool>(args[4].value));
    CHECK(!std::any_cast<bool>(args[5].value)); RE::PlayerCharacter::instance=nullptr; CHECK(!vm.calls.back().args(args));
}
TEST(emergency_recovery_cleans_resolvable_state_and_retains_unresolved_owned_resources) {
    RE::Actor actor(1), aiOnly(2); Add(actor); Add(aiOnly);
    CHECK(Capture::CaptureActor(&actor,nullptr)); CHECK(Recovery::AcquireLease(&actor,"owner",50));
    Defeat::SuspendAI(&aiOnly); Defeat::RestoreAISuspension(99,false); Engine::RestoreCaptiveFaction(99,123);
    DefeatRegistry::Add(99,1,0); Capture::RestoreCapture(99,1,1,true);
    CHECK(Defeat::ForceRecoverAll()==1); CHECK(DefeatRegistry::Snapshot().empty()); CHECK(Capture::Snapshot().empty());
    CHECK(aiOnly.boolBits.enabled); CHECK(Defeat::SnapshotAISuspensions()==std::vector<std::pair<FormID,bool>>({{99,false}}));
    CHECK(Engine::SnapshotCaptiveFactions()==std::vector<std::pair<FormID,FormID>>({{99,123}}));
}
TEST(context_resolves_current_forms_and_clears_all_fields) {
    RE::Actor actor(1); Add(actor); Consequence::PlayerConsequence c; c.id="id"; c.displayName="name"; c.quest="quest";
    c.recoveryMode=Consequence::RecoveryMode::kHeld;
    DefeatContext::SetPlayerContext(&actor,c,{&actor},DefeatCause::kHealth);
    CHECK(DefeatContext::GetLastPlayerConsequenceId()=="id"); CHECK(DefeatContext::GetLastPlayerConsequenceName()=="name");
    CHECK(DefeatContext::GetLastPlayerConsequenceQuest()=="quest"); CHECK(DefeatContext::IsLastPlayerRecoveryHeld());
    CHECK(DefeatContext::GetLastPlayerAggressor()==&actor); CHECK(DefeatContext::GetLastPlayerDefeatCause()==DefeatCause::kHealth);
    RE::TESForm::forms.clear(); CHECK(!DefeatContext::GetLastPlayerAggressor()); CHECK(DefeatContext::GetLastPlayerRecentHostileAttackers().empty());
    DefeatContext::ClearPlayerContext(); CHECK(DefeatContext::GetLastPlayerConsequenceId().empty());
    CHECK(DefeatContext::GetLastPlayerAutoRecoveryDelaySeconds()==0); CHECK(!DefeatContext::IsLastPlayerRecoveryHeld());
}
