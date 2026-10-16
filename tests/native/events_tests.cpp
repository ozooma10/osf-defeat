#include "Test.h"
using namespace Test;

TEST(registration_validation_unsubscribe_and_slot_generation) {
    auto receiver=std::make_shared<RE::BSScript::Object>();
    CHECK(!Events::RegisterPlayerDefeated({},"callback")); CHECK(!Events::RegisterPlayerDefeated(receiver,""));
    const auto old=Events::RegisterPlayerDefeated(receiver,"old"); CHECK(old!=0);
    Events::BroadcastPlayerDefeated(); CHECK(vm.calls.size()==1 && vm.calls[0].name=="old");
    CHECK(Events::UnregisterPlayerDefeated(old)); CHECK(!Events::UnregisterPlayerDefeated(old));
    const auto current=Events::RegisterPlayerDefeated(receiver,"current"); CHECK(current!=old);
    CHECK(!Events::UnregisterPlayerDefeated(old)); Events::BroadcastPlayerDefeated(); CHECK(vm.calls.back().name=="current");
    CHECK(!Events::UnregisterPlayerDefeated(0)); CHECK(!Events::UnregisterPlayerDefeated(123));
}
TEST(all_channels_dispatch_and_clear_unregisters_every_receiver) {
    auto r=std::make_shared<RE::BSScript::Object>();
    Events::RegisterPlayerDefeated(r,"pd"); Events::RegisterPlayerRecovered(r,"pr");
    Events::RegisterPlayerConsequenceStarted(r,"pc"); Events::RegisterPlayerDefeatImminent(r,"pi");
    Events::RegisterActorDefeated(r,"ad"); Events::RegisterActorRecovered(r,"ar");
    auto broadcast=[] { Events::BroadcastPlayerDefeated(); Events::BroadcastPlayerRecovered(); Events::BroadcastPlayerConsequenceStarted();
        Events::BroadcastPlayerDefeatImminent(); Events::BroadcastActorDefeated(1,2); Events::BroadcastActorRecovered(1,2); };
    broadcast(); CHECK(vm.calls.size()==6);
    std::vector<std::string> names; for(const auto& call:vm.calls) names.push_back(call.name);
    CHECK(names==std::vector<std::string>({"pd","pr","pc","pi","ad","ar"}));
    Events::Clear(); broadcast(); CHECK(vm.calls.size()==6);
}
TEST(actor_arguments_resolve_when_vm_packs_them) {
    auto r=std::make_shared<RE::BSScript::Object>(); Events::RegisterActorDefeated(r,"callback");
    RE::Actor victim(1), aggressor(2); Add(victim); Add(aggressor);
    Events::BroadcastActorDefeated(1,2); RE::TESForm::forms.erase(2);
    RE::BSScrapArray<RE::BSScript::Variable> args; CHECK(vm.calls[0].args(args)); CHECK(args.size()==2);
    CHECK(std::any_cast<RE::Actor*>(args[0].value)==&victim); CHECK(std::any_cast<RE::Actor*>(args[1].value)==nullptr);
}
TEST(unavailable_vm_does_not_lose_subscriptions) {
    auto r=std::make_shared<RE::BSScript::Object>(); auto token=Events::RegisterPlayerRecovered(r,"callback");
    RE::GameVM::instance=nullptr; Events::BroadcastPlayerRecovered(); CHECK(vm.calls.empty());
    RE::GameVM::instance=&gameVM; Events::BroadcastPlayerRecovered(); CHECK(vm.calls.size()==1);
    CHECK(Events::UnregisterPlayerRecovered(token)); Events::BroadcastPlayerRecovered(); CHECK(vm.calls.size()==1);
}
