#include "pch.h"
#include "AcceptanceTests.h"
#include "Core/Capture.h"
#include "Core/Config.h"
#include "Core/Consequence.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/Recovery.h"
#include "Core/RuntimeMutation.h"
#include "Engine/Engine.h"
#include "External/OSFSceneAPI.h"
#include "RE/B/BSScriptUtil.h"
#include "RE/E/Events.h"
#include "RE/V/VirtualMachine.h"
#include "RE/P/PlayerCamera.h"
#include "RE/N/NiNode.h"
#include <mutex>
#include <variant>

namespace OSFDefeat::RuntimeTests::Acceptance {
namespace {
    using Json=nlohmann::json;
    using VM=RE::BSScript::IVirtualMachine;
    constexpr auto Script="OSFDefeatAcceptance";
    std::mutex observationsLock;
    Json records=Json::array();
    std::uint64_t sequence{};
    std::atomic<std::uint64_t> saves{}, loads{}, reattachments{};
    std::atomic<FormID> npcID{};
    int phase{};
    std::int64_t phaseAt{};
    std::uint64_t actionId{}, beforeSave{}, beforeLoad{}, beforeEpoch{}, beforeReattach{}, beforeRecords{};
    std::uint64_t acknowledgedId{};
    RE::NiPoint3 beforePosition{};
    std::array<float,9> beforeCamera{};
    float baselineDistance{}, transferDistance{};
    ActorStateSnapshot npcPrior{};
    std::string action;
    struct PackCase { const char* name; const char* file; const char* id; };
    constexpr PackCase packCases[]={
        {"robbed","basic-robbed.json","osfdefeat.basic.robbed_and_left"},
        {"picked-clean","basic-pickedclean.json","osfdefeat.basic.picked_clean"},
        {"message","basic-message.json","osfdefeat.basic.left_as_a_message"},
        {"scrapped","basic-scrapped.json","osfdefeat.basic.left_for_scrap"},
        {"mauled","basic-mauled.json","osfdefeat.basic.mauled"},
        {"arrest-fallback","basic-arrested.json","osfdefeat.basic.taken_into_custody"}
    };
    std::size_t packIndex{};
    bool packDefeatObserved{};
    std::int64_t Now() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
    void Record(VM&,std::uint32_t,std::monostate,RE::BSFixedString name,bool ok,RE::Actor* subject,float value) {
        std::scoped_lock lock(observationsLock);
        // Papyrus interns strings case-insensitively and may return the casing
        // of an earlier interned value (for example "Message").
        std::string label=name.c_str();
        for(auto& ch:label) if(ch>='A' && ch<='Z') ch=static_cast<char>(ch-'A'+'a');
        records.push_back({{"sequence",++sequence},{"name",label},{"ok",ok},
            {"subject",subject?subject->GetFormID():0},{"value",value}});
        if(label=="fixture" && ok && subject) npcID=subject->GetFormID();
    }
    bool Saw(std::string_view name,std::uint64_t after=0,FormID subject=0) {
        std::scoped_lock lock(observationsLock);
        for(const auto& record:records) if(record.at("name").get<std::string>()==name && record.at("sequence").get<std::uint64_t>()>after &&
            record.at("ok").get<bool>() && (!subject || record.at("subject")==subject)) return true;
        return false;
    }
    std::uint64_t Sequence() { std::scoped_lock lock(observationsLock); return sequence; }
    std::size_t EventCount() {
        std::scoped_lock lock(observationsLock); std::size_t count=0;
        for(const auto& record:records) if(record.at("name").get<std::string>().starts_with("event-")) ++count;
        return count;
    }
    class EngineEvents final: public RE::BSTEventSink<RE::SaveLoadEvent>,public RE::BSTEventSink<RE::TESLoadGameEvent> {
        RE::BSEventNotifyControl ProcessEvent(const RE::SaveLoadEvent& event,RE::BSTEventSource<RE::SaveLoadEvent>*) override {
            std::scoped_lock lock(observationsLock);
            records.push_back({{"sequence",++sequence},{"name","engine-save-load"},{"ok",true},{"op",static_cast<int>(event.opType)},{"status",static_cast<int>(event.status)}});
            return RE::BSEventNotifyControl::kContinue;
        }
        RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent&,RE::BSTEventSource<RE::TESLoadGameEvent>*) override {
            ++loads; return RE::BSEventNotifyControl::kContinue;
        }
    } engineEvents;
    void Papyrus(std::string_view function,std::string argument={}) {
        auto* vm=RE::GameVM::GetSingleton()->GetVM(); auto* player=RE::PlayerCharacter::GetSingleton();
        const auto handle=vm->GetObjectHandlePolicy().GetHandleForObject(static_cast<std::uint32_t>(player->GetFormType()),player);
        RE::BSTSmartPointer<RE::BSScript::Object> object;
        if(!vm->FindBoundObject(handle,Script,false,object,true)) {
            if(!vm->CreateObject(Script,object) || !object) throw std::runtime_error("Cannot create Papyrus acceptance consumer");
            RE::BSScript::Internal::VirtualMachine::GetSingleton()->BindObject(object,handle,false);
            RE::BSTSmartPointer<RE::BSScript::Object> bound;
            if(!vm->FindBoundObject(handle,Script,false,bound,true) || bound.get()!=object.get()) throw std::runtime_error("Cannot bind Papyrus acceptance consumer");
        }
        const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        if(!vm->DispatchMethodCall(handle,Script,RE::BSFixedString(function),[argument](RE::BSScrapArray<RE::BSScript::Variable>& args){
            args.clear(); if(!argument.empty()){args.resize(1);RE::BSScript::PackVariable(args[0],RE::BSFixedString(argument));} return true;
        },callback,0))
            throw std::runtime_error("Papyrus acceptance submission refused");
    }
    void Check(Json& out,std::string_view name,bool passed) {
        out["cases"].push_back({{"name",name},{"passed",passed}});
        if(!passed) throw std::runtime_error(std::string(name));
    }
    void Next() { ++phase; phaseAt=Now(); }
    bool Wait(bool ready,std::int64_t seconds=20) {
        if(!ready && Now()-phaseAt>seconds*1000) throw std::runtime_error("Acceptance phase "+std::to_string(phase)+" timed out");
        return !ready;
    }
    float Distance(RE::NiPoint3 a,RE::NiPoint3 b) { return std::sqrt((a-b).SqrLength()); }
    std::array<float,9> CameraRotation() {
        const auto* camera=RE::PlayerCamera::GetSingleton();
        if(!camera || !camera->cameraRoot) throw std::runtime_error("Camera root unavailable");
        std::array<float,9> result{};
        for(std::size_t i=0;i<3;++i) {
            const auto& row=camera->cameraRoot->world.rotate.entry[i];
            result[i*3]=row.x; result[i*3+1]=row.y; result[i*3+2]=row.z;
        }
        return result;
    }
    bool Ghost(RE::Actor* a) { return a->extraDataList && a->extraDataList->HasType(RE::ExtraDataType::kGhost); }
    bool Down(RE::Actor* a) { return a && !a->IsDead() && Defeat::IsDefeated(a) && Ghost(a) && a->boolFlags.all(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery); }
    bool Loaded(RE::Actor* a) { if(!a) return false; const auto loaded=a->loadedData.LockRead(); return *loaded && (*loaded)->data3D; }
    void Request(Json& out,std::string_view kind,std::string_view name) {
        action=std::string(kind); ++actionId;
        beforeSave=saves; beforeLoad=loads; beforeEpoch=RuntimeMutation::CurrentEpoch();
        out["action"]={{"id",actionId},{"kind",kind},{"name",name}};
    }
    bool Acknowledged(Json& out,const std::filesystem::path& directory) {
        if(acknowledgedId==actionId) return true;
        std::ifstream input(directory/"action-result.json"); if(!input) return false;
        const auto result=Json::parse(input);
        if(result.value("id",0ULL)!=actionId) return false;
        for(const auto* key:{"pid","processStartFileTime","launchToken","runId"})
            if(result.at(key)!=out.at(key)) throw std::runtime_error("Action receipt belongs to another runtime");
        const auto expires=result.at("expiresAtUtcMs").get<std::int64_t>();
        if(expires<Now() || expires>Now()+15000) throw std::runtime_error("Action receipt expired or future dated");
        if(!result.at("ok").get<bool>()) throw std::runtime_error("Runner action failed");
        if(action=="save" && saves<=beforeSave) return false;
        if(action=="load" && (loads<=beforeLoad || RuntimeMutation::CurrentEpoch()<=beforeEpoch)) return false;
        acknowledgedId=actionId;
        out.erase("action"); return true;
    }
    void Damage(RE::Actor* actor,float delta,RE::Actor* aggressor=nullptr) {
        using Fn=void(*)(RE::ActorValueOwner*,RE::ACTOR_VALUE_MODIFIER,const RE::ActorValueInfo&,float,RE::TESObjectREFR*);
        REL::Relocation<Fn> fn{REL::ID{101587}}; fn(&Engine::AVOwnerOf(*actor),RE::ACTOR_VALUE_MODIFIER::kDamage,*Engine::HealthAV(),delta,aggressor);
    }
}
void Bind(VM& vm) { vm.BindNativeMethod("OSFDefeatAcceptanceProbe","Record",&Record,std::nullopt,false); }
void Reattached(std::uint32_t actor) { if(actor==npcID.load()) ++reattachments; }
void Saved() { ++saves; }
bool Tick(Json& out,RE::Actor* player,OSF::API::IOSFSceneAPI* scenes,const std::filesystem::path& directory) {
    out["acceptancePhase"]=phase;
    out["engine"]={{"saveCallbacks",saves.load()},{"loaded",loads.load()},{"epoch",RuntimeMutation::CurrentEpoch()},{"npcReattachments",reattachments.load()}};
    { std::scoped_lock lock(observationsLock); out["papyrus"]=records; }
    auto* npc=RE::TESForm::LookupByID<RE::Actor>(npcID.load());
    if(npc) out["npc"]={{"id",npc->GetFormID()},{"defeated",Defeat::IsDefeated(npc)},{"captive",Capture::IsCaptive(npc)},
        {"faction",Engine::IsInCaptiveFaction(*npc)},{"distance",Distance(npc->GetPosition(),player->GetPosition())}};
    switch(phase) {
    case 0:
        RE::SaveLoadEvent::GetEventSource()->RegisterSink(&engineEvents);
        RE::TESLoadGameEvent::GetEventSource()->RegisterSink(&engineEvents);
        Papyrus("Initialize"); Next(); break;
    case 1:
        if(Wait(Saw("fixture") && Saw("registered") && Loaded(npc))) break;
        Check(out,"npc-disposable-fixture",npc!=player && !npc->IsDead() && !Engine::IsInCaptiveFaction(*npc));
        Check(out,"papyrus-registration",Saw("registered"));
        beforePosition=player->GetPosition(); Request(out,"move","baseline"); Next(); break;
    case 2:
        if(Wait(Acknowledged(out,directory))) break;
        baselineDistance=Distance(beforePosition,player->GetPosition());
        out["movementEvidence"]["baseline"]=baselineDistance;
        Check(out,"baseline-movement",baselineDistance>0.25f);
        Request(out,"save","osfdefeathealthy"); Next(); break;
    case 3:
        if(Wait(Acknowledged(out,directory),60)) break;
        Check(out,"healthy-save",DefeatRegistry::Snapshot().empty());
        Check(out,"npc-capture",Capture::CaptureActor(npc,player) && Down(npc) && Capture::GetCaptorID(npc)==player->GetFormID() &&
            Capture::GetCaptives(player).size()==1 && Engine::HasCaptiveFactionApplied(*npc) && Engine::IsInCaptiveFaction(*npc));
        npcPrior=DefeatRegistry::Get(npc->GetFormID())->priorState;
        Defeat::SuspendAI(npc);
        if(!Defeat::DefeatActor(player,npc) || !Recovery::AcquireLease(player,"acceptance-save-load",300)) throw std::runtime_error("Cannot prepare held save");
        Next(); break;
    case 4:
        if(Wait(scenes->GetSceneForActor(player)!=0 && Saw("event-player-down") && Saw("event-actor-down",0,npcID))) break;
        Papyrus("InspectDownControls"); beforePosition=player->GetPosition(); Request(out,"move","down"); Next(); break;
    case 5:
        if(Wait(Acknowledged(out,directory) && Saw("controls-down"))) break;
        out["movementEvidence"]["down"]=Distance(beforePosition,player->GetPosition());
        Check(out,"down-input-lock",Distance(beforePosition,player->GetPosition())<std::max(0.05f,baselineDistance*0.1f) && Saw("controls-down"));
        Request(out,"save","osfdefeatdown"); Next(); break;
    case 6:
        if(Wait(Acknowledged(out,directory),60)) break;
        Check(out,"defeated-and-captive-save",Down(player) && Down(npc) && Capture::IsCaptive(npc) && Defeat::IsAISuspended(npc));
        Check(out,"mutate-before-reload",Defeat::ForceRecoverAll()==2 && Capture::Snapshot().empty());
        Request(out,"load","osfdefeatdown"); Next(); break;
    case 7:
        if(Wait(Acknowledged(out,directory) && Loaded(npc) && Down(player) && Down(npc) && scenes->GetSceneForActor(player)!=0,60)) break;
        Check(out,"player-real-save-load",Recovery::GetLeaseHolder(player)=="acceptance-save-load" && Recovery::SecondsRemaining(player)<0);
        Check(out,"npc-capture-real-save-load",Capture::IsCaptive(npc) && Capture::GetCaptorID(npc)==player->GetFormID() && Capture::GetCaptives(player).size()==1 &&
            Engine::HasCaptiveFactionApplied(*npc) && Engine::IsInCaptiveFaction(*npc) && Defeat::IsAISuspended(npc));
        Check(out,"loaded-player-scene",scenes->GetSceneForActor(player)!=0);
        beforeReattach=reattachments; Papyrus("RebuildNpc"); Next(); break;
    case 8:
        if(Wait(reattachments>beforeReattach && Loaded(npc) && Saw("npc-rebuilt"),30)) break;
        Check(out,"npc-3d-reattach",Down(npc) && Capture::IsCaptive(npc) && Engine::IsInCaptiveFaction(*npc));
        transferDistance=Distance(npc->GetPosition(),player->GetPosition());
        out["movementEvidence"]["npcBeforeTransfer"]=transferDistance;
        Check(out,"npc-transfer-starts-away",transferDistance>2);
        if(!Capture::TransferCaptive(npc,player)) throw std::runtime_error("Transfer submission refused"); Next(); break;
    case 9:
        // Actor collision can separate co-located actors after MoveTo (0.62 in
        // this fixture). Require arrival within one unit and a substantial move.
        if(Wait(Distance(npc->GetPosition(),player->GetPosition())<1.0f &&
            Distance(npc->GetPosition(),player->GetPosition())<transferDistance*0.25f)) break;
        out["movementEvidence"]["npcAfterTransfer"]=Distance(npc->GetPosition(),player->GetPosition());
        Check(out,"npc-transfer-moves",npc->parentCell==player->parentCell && Capture::IsCaptive(npc));
        Check(out,"npc-release-restores-state",Capture::ReleaseCaptive(npc,true) && !Defeat::IsDefeated(npc) && !Capture::IsCaptive(npc) &&
            !Engine::HasCaptiveFactionApplied(*npc) && !Engine::IsInCaptiveFaction(*npc) && Ghost(npc)==npcPrior.ghost &&
            npc->boolBits.all(RE::Actor::BOOL_BITS::kProcessMe)==npcPrior.aiEnabled && !Defeat::IsAISuspended(npc));
        if(!Recovery::ScheduleAutomatic(player,3)) throw std::runtime_error("Loaded recovery timer refused"); Next(); break;
    case 10:
        if(Wait(!Defeat::IsDefeated(player) && scenes->GetSceneForActor(player)==0)) break;
        Check(out,"recovery-after-load",!Ghost(player));
        Papyrus("InspectRecoveredControls"); beforePosition=player->GetPosition(); Request(out,"move","recovered"); Next(); break;
    case 11:
        if(Wait(Acknowledged(out,directory) && Saw("controls-up"))) break;
        out["movementEvidence"]["recovered"]=Distance(beforePosition,player->GetPosition());
        Check(out,"recovered-movement-controls",Distance(beforePosition,player->GetPosition())>baselineDistance*0.25f && Saw("controls-up"));
        beforeCamera=CameraRotation(); Request(out,"look","recovered"); Next(); break;
    case 12:
        if(Wait(Acknowledged(out,directory))) break;
        {
            const auto afterCamera=CameraRotation();
            float delta=0;
            for(std::size_t i=0;i<afterCamera.size();++i) delta+=std::abs(beforeCamera[i]-afterCamera[i]);
            out["movementEvidence"]["cameraBefore"]=beforeCamera;
            out["movementEvidence"]["cameraAfter"]=afterCamera;
            out["movementEvidence"]["lookRotationDelta"]=delta;
            if(delta<=0.001f && Now()-phaseAt<5000) break;
            Check(out,"recovered-looking",delta>0.001f);
        }
        beforeRecords=Sequence(); Papyrus("RegisterCallbacks"); Next(); break;
    case 13:
        if(Wait(Saw("registered",beforeRecords))) break;
        Check(out,"papyrus-registration-after-load",Saw("registered",beforeRecords));
        if(!Defeat::DefeatActor(npc,player)) throw std::runtime_error("NPC callback defeat failed"); Next(); break;
    case 14:
        if(Wait(Saw("event-actor-down",beforeRecords,npcID))) break;
        Defeat::RecoverActor(npc,true);
        { auto& owner=Engine::AVOwnerOf(*player); Damage(player,owner.GetPermanentActorValue(*Engine::HealthAV())*0.05f-owner.GetActorValue(*Engine::HealthAV()),npc); }
        Next(); break;
    case 15:
        if(Wait(Saw("event-imminent",beforeRecords) && Saw("event-actor-up",beforeRecords,npcID))) break;
        Damage(player,-Engine::AVOwnerOf(*player).GetActorValue(*Engine::HealthAV())-100,npc); Next(); break;
    case 16:
        if(Wait(Saw("event-consequence",beforeRecords) && Saw("event-player-down",beforeRecords))) break;
        Defeat::ForceRecoverAll(); Next(); break;
    case 17:
        if(Wait(Saw("event-player-up",beforeRecords))) break;
        Check(out,"papyrus-all-six-callbacks",Saw("event-actor-down",beforeRecords,npcID) && Saw("event-actor-up",beforeRecords,npcID) &&
            Saw("event-player-down",beforeRecords) && Saw("event-player-up",beforeRecords) && Saw("event-imminent",beforeRecords) && Saw("event-consequence",beforeRecords));
        Papyrus("UnregisterCallbacks"); Next(); break;
    case 18:
        if(Wait(Saw("unregistered",beforeRecords))) break;
        Check(out,"papyrus-unregister",Saw("unregistered",beforeRecords));
        beforeRecords=EventCount(); Defeat::DefeatActor(npc,player); Defeat::RecoverActor(npc,true); Defeat::DefeatActor(player,npc); Defeat::ForceRecoverAll(); Next(); break;
    case 19:
        if(Now()-phaseAt<3000) break;
        Check(out,"papyrus-unsubscribe-stops-delivery",EventCount()==beforeRecords);
        if(!Capture::CaptureActor(npc,player) || !Defeat::DefeatActor(player,npc) || !Recovery::AcquireLease(player,"healthy-reload",300)) throw std::runtime_error("Healthy reload setup failed");
        Request(out,"load","osfdefeathealthy"); Next(); break;
    case 20:
        if(Wait(Acknowledged(out,directory) && Loaded(npc) && !Defeat::IsDefeated(player) && scenes->GetSceneForActor(player)==0,60)) break;
        Check(out,"healthy-load-clears-defeat-and-capture",DefeatRegistry::Snapshot().empty() && Capture::Snapshot().empty() &&
            Engine::SnapshotCaptiveFactions().empty() && Defeat::SnapshotAISuspensions().empty() && !Ghost(player) && !Ghost(npc));
        Next(); break;
    case 21:
        if(Now()-phaseAt<5000) break;
        Check(out,"healthy-load-no-stale-reassert",!Defeat::IsDefeated(player) && scenes->GetSceneForActor(player)==0 && !Capture::IsCaptive(npc));
        Request(out,"save","osfdefeatclean"); Next(); break;
    case 22:
        if(Wait(Acknowledged(out,directory),60)) break;
        Check(out,"clean-save-no-defeat-records",DefeatRegistry::Snapshot().empty() && Capture::Snapshot().empty());
        Next(); break;
    case 23:
        // Use one unchanged authored pack definition at a time for deterministic
        // acceptance. Keyword/faction gates still run against the real fixture NPC.
        std::filesystem::copy_file(Config::RootPath()/"AcceptancePack"/packCases[packIndex].file,
            Config::RootPath()/"Consequences"/"Player"/"zz-acceptance-pack.json",std::filesystem::copy_options::overwrite_existing);
        Consequence::Reload();
        Papyrus("PreparePack",packCases[packIndex].name); packDefeatObserved=false; Next(); break;
    case 24:
        if(Wait(Saw(std::string("pack-prepared-")+packCases[packIndex].name))) break;
        Damage(player,-Engine::AVOwnerOf(*player).GetActorValue(*Engine::HealthAV())-100,npc); Next(); break;
    case 25:
        if(Defeat::IsDefeated(player) && DefeatContext::GetLastPlayerConsequenceId()==packCases[packIndex].id) packDefeatObserved=true;
        if(Wait(packDefeatObserved && !Defeat::IsDefeated(player) && scenes->GetSceneForActor(player)==0,45)) break;
        Papyrus("InspectPack"); Next(); break;
    case 26:
        if(Wait(Saw(std::string("pack-effect-")+packCases[packIndex].name))) break;
        Check(out,std::string("pack-")+packCases[packIndex].name,true);
        if(++packIndex<std::size(packCases)) {phase=23;phaseAt=Now();break;}
        std::filesystem::remove(Config::RootPath()/"Consequences"/"Player"/"zz-acceptance-pack.json"); Consequence::Reload();
        if(!Defeat::DefeatActor(player,npc) || !Recovery::AcquireLease(player,"marker-test",60)) throw std::runtime_error("Marker setup failed");
        Papyrus("MarkerRecovery"); Next(); break;
    case 27:
        if(Wait(Saw("marker-recovery") && !Defeat::IsDefeated(player) && scenes->GetSceneForActor(player)==0)) break;
        Check(out,"pack-recovery-marker-moves-player",Saw("marker-recovery"));
        Request(out,"load","osfdefeathealthy"); Next(); break;
    case 28:
        if(Wait(Acknowledged(out,directory) && !Defeat::IsDefeated(player) && scenes->GetSceneForActor(player)==0,60)) break;
        Check(out,"pack-cleanup-load",DefeatRegistry::Snapshot().empty() && Capture::Snapshot().empty());
        return true;
    }
    return false;
}
}
