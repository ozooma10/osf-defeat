#include "Test.h"
#include "Core/PlayerDefeat.h"
#include "Core/Presentation.h"

namespace OSFDefeat::Engine {
    FormID PrimaryWeaponFormID(RE::Actor&) { return Test::primaryWeaponID; }
    RE::ActorValueInfo* HealthAV() { static RE::ActorValueInfo health; return &health; }
    RE::ActorValueOwner& AVOwnerOf(RE::Actor& actor) { return actor; }
    RE::Actor* ResolveCombatTarget(RE::Actor* actor) { return actor ? actor->combatTarget : nullptr; }
    // Observe calls and emulate only the engine-owned state. Production Core/*.cpp
    // remains responsible for all registry, ownership, transaction and event decisions.
    void ApplyDownState(RE::Actor& actor, ActiveDefeat& data) {
        ++Test::downCalls;
        if (!data.priorState.valid) { data.priorState.valid = true; data.priorState.aiEnabled = actor.boolBits.enabled; }
    }
    void StopCombat(RE::Actor&) {}
    void RestoreState(RE::Actor& actor, const ActiveDefeat&, bool preserveFaction) {
        ++Test::restoreCalls;
        if (!preserveFaction) RemoveCaptiveFaction(actor);
        if (Test::onRestore) Test::onRestore(actor);
    }
    bool IsCaptiveFactionAvailable() { return Test::factionAvailable; }
    bool ApplyCaptiveFaction(RE::Actor& actor) {
        if (!Test::factionApplyWorks) return false;
        if (!IsInCaptiveFaction(actor)) Test::ownedFactions[actor.id] = Config::CaptiveFactionID();
        return true;
    }
    bool RemoveCaptiveFaction(RE::Actor& actor) { return Test::ownedFactions.erase(actor.id) != 0; }
    bool HasCaptiveFactionApplied(RE::Actor& actor) { return Test::ownedFactions.contains(actor.id); }
    bool IsInCaptiveFaction(RE::Actor& actor) { return HasCaptiveFactionApplied(actor) || Test::preexistingFactions.contains(actor.id); }
    std::vector<std::pair<FormID, FormID>> SnapshotCaptiveFactions() { return {Test::ownedFactions.begin(), Test::ownedFactions.end()}; }
    void RestoreCaptiveFaction(FormID actor, FormID faction) { Test::ownedFactions[actor] = faction; }
    void ResetRuntimeState() { Test::ownedFactions.clear(); }
}
namespace OSFDefeat::Presentation {
    void ShowPlayerDown() { ++Test::showCalls; if(Test::onShow) Test::onShow(); }
    void ClearPlayerDown() { ++Test::clearCalls; }
    void ReassertPlayerDown() { ++Test::reassertCalls; }
    void ResetRuntimeState() { ++Test::resetCalls; }
}
namespace OSFDefeat::QuestHandoff {
    bool TryStartQuestToken(std::string_view token) {
        Test::questContextAtStart = DefeatContext::GetLastPlayerConsequenceId();
        Test::startedQuest = token; return true;
    }
}
namespace Test {
    void Reset() {
        auto* queue = RE::BSService::TaskQueue::GetSingleton(); queue->pending.clear(); queue->inlineTasks = false;
        TestEngine::queueEnabled = 1;
        (void)RuntimeMutation::AdvanceEpoch();
        Defeat::ResetRuntimeState(); Capture::Clear(); Engine::ResetRuntimeState();
        Events::Clear(); DefeatContext::ClearPlayerContext(); Consequence::ResetRuntimeState(); Eligibility::SetEnabled(true);
        PlayerDefeat::ResetTransientState(); startedQuest.clear(); questContextAtStart.clear();
        RE::TESForm::forms.clear(); RE::TESForm::editors.clear(); RE::PlayerCharacter::instance = nullptr;
        calendar = {}; RE::Calendar::instance = &calendar;
        vm = {}; gameVM.vm = &vm; RE::GameVM::instance = &gameVM;
        primaryWeaponID=0; onShow={};
        factionAvailable = factionApplyWorks = true; preexistingFactions.clear(); onRestore = {}; onOriginalDamage = {};
        downCalls = restoreCalls = showCalls = clearCalls = reassertCalls = resetCalls = 0;
        TestEngine::logs.clear(); TestEngine::moduleAvailable = true; TestEngine::persistenceExport = nullptr;
        const auto root = Config::RootPath();
        if (root != sandbox / "Data" / "SFSE" / "OSFDefeat") throw std::runtime_error("Fixture path escaped sandbox");
        std::filesystem::remove_all(root); std::filesystem::create_directories(root);
        Config::Load(); Consequence::Load();
    }
}
