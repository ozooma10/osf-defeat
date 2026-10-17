#include "pch.h"
#include "DefeatRuntimeTests.h"
#include "AcceptanceTests.h"
#include "Core/Config.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/Recovery.h"
#include "Core/Eligibility.h"
#include "Engine/Engine.h"
#include "External/OSFSceneAPI.h"
#include "RE/I/INISettingCollection.h"
#include <Windows.h>
#undef ERROR
#include <nlohmann/json.hpp>

// Test-build-only driver. Every mutation executes after the verified Main update,
// requires a fresh request for this exact process, and ends at a bounded deadline.
namespace OSFDefeat::RuntimeTests {
    namespace {
        using Json = nlohmann::json;
        std::filesystem::path directory;
        Json snapshot;
        std::string token, processStart;
        bool initialized=false, running=false, finished=false, shadowsOnly=false, blackoutColdStart=false;
        int phase=0;
        std::int64_t phaseStart{}, deadline{};
        float initialHealth{};
        OSF::API::IOSFSceneAPI* scenes=nullptr;
        std::int64_t Now() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
        void Publish() {
            snapshot["updatedAtUtcMs"]=Now(); snapshot["phase"]=phase;
            const auto temporary=directory / "snapshot.tmp";
            { std::ofstream stream(temporary); stream << snapshot.dump(2); if(!stream) throw std::runtime_error("Cannot publish runtime snapshot"); }
            // Readers share deletion, but a filesystem observer can still hold
            // a transient handle. Keep replacement atomic and bound retries to
            // 20 ms; one sharing collision must not tear down a valid game run.
            DWORD error=0;
            for(int attempt=0;attempt<11;++attempt) {
                if(MoveFileExW(temporary.c_str(),(directory / "snapshot.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return;
                error=GetLastError();
                if(error!=ERROR_SHARING_VIOLATION && error!=ERROR_ACCESS_DENIED) break;
                if(attempt<10) Sleep(2);
            }
            throw std::runtime_error("Cannot replace runtime snapshot (Win32 "+std::to_string(error)+")");
        }
        void Check(std::string_view name,bool passed) {
            snapshot["cases"].push_back({{"name",name},{"passed",passed}});
            if(!passed) throw std::runtime_error(std::string(name));
        }
        void Next() { ++phase; phaseStart=Now(); }
        void Cleanup(RE::Actor* player) {
            Defeat::ForceRecoverAll(); Eligibility::SetEnabled(true);
            if(player && Engine::HealthAV()) {
                auto& owner=Engine::AVOwnerOf(*player);
                owner.RestoreActorValue(*Engine::HealthAV(),std::max(0.0f,initialHealth-owner.GetActorValue(*Engine::HealthAV())));
            }
        }
    }
    void Tick() {
        try {
            if(!initialized) {
                initialized=true;
                std::ifstream input(Config::RootPath().parent_path() / "Plugins" / "OSFDefeatTest.json");
                if(!input) return;
                const auto config=Json::parse(input);
                shadowsOnly=config.value("shadowsOnly",false);
                blackoutColdStart=config.value("blackoutColdStart",false);
                directory=config.at("stateDir").get<std::string>(); token=config.at("launchToken").get<std::string>();
                if(!directory.is_absolute() || token.empty() || !std::filesystem::is_directory(directory)) { directory.clear(); return; }
                FILETIME created{}, exited{}, kernel{}, user{};
                if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)) throw std::runtime_error("Process identity unavailable");
                processStart=std::to_string((static_cast<std::uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime);
                snapshot={{"schemaVersion",1},{"pid",GetCurrentProcessId()},{"processStartFileTime",processStart},
                    {"launchToken",token},{"state","ready"},{"cases",Json::array()},{"runId",""}};
                // OSF Animation resolves its sidecar directory from this collection. The
                // engine may load the profile save path through a different INI layer.
                // Require the actual engine setting to point at our owned fixture before any save.
                const auto savePath=config.at("savePath").get<std::string>();
                auto* ini=RE::INISettingCollection::GetSingleton();
                snapshot["savePathSettings"]=Json::array();
                if(ini) for(auto* setting:ini->settings) {
                    std::string key(setting->GetKey()); auto lower=key;
                    std::ranges::transform(lower,lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                    if(lower.find("localsavepath")!=std::string::npos) snapshot["savePathSettings"].push_back({{"key",key},{"value",setting->GetString()}});
                }
                auto* setting=ini?ini->GetSetting("sLocalSavePath:General"):nullptr;
                if(!setting && ini) setting=ini->GetSetting("SLocalSavePath:General");
                if(!setting || setting->GetType()!=RE::Setting::Type::kString) throw std::runtime_error("OSF Animation sidecar save-path setting unavailable; refusing shared Saves fallback");
                snapshot["sidecarPathBefore"]=setting->GetString();
                const auto logs=SFSE::log::log_directory();
                const auto actual=logs->parent_path().parent_path()/std::filesystem::path(std::string(setting->GetString()));
                if(!std::filesystem::equivalent(actual,std::filesystem::path(savePath))) throw std::runtime_error("Engine save directory differs from private fixture");
                snapshot["sidecarPath"]=setting->GetString();
                Publish();
            }
            if(directory.empty() || finished) return;
            auto* player=static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton());
            if(!running) {
                std::ifstream input(directory / "start.json"); if(!input) return;
                const auto request=Json::parse(input);
                const auto expires=request.at("expiresAtUtcMs").get<std::int64_t>();
                if(request.value("pid",0u)!=GetCurrentProcessId() || request.value("processStartFileTime",std::string{})!=processStart ||
                    request.value("launchToken",std::string{})!=token || expires<=Now() || expires>Now()+15000) return;
                snapshot["runId"]=request.at("runId"); snapshot["state"]="running";
                running=true; deadline=Now()+600000; phaseStart=Now();
                scenes=OSF::API::RequestSceneAPI();
                if(shadowsOnly || blackoutColdStart) {
                    if(!player || !Engine::HealthAV() || !scenes) throw std::runtime_error("Runtime dependencies unavailable");
                    if(!blackoutColdStart) Check("healthy-baseline",!Defeat::IsDefeated(player));
                    initialHealth=Engine::AVOwnerOf(*player).GetActorValue(*Engine::HealthAV());
                    phase=8;
                }
            }
            if(Now()>deadline) throw std::runtime_error("Runtime suite deadline expired");
            if(auto* ui=RE::UI::GetSingleton(); ui && (ui->IsMenuOpen("LoadingMenu") || ui->IsMenuOpen("MainMenu"))) { Publish(); return; }
            if(!player || player->IsDead()) throw std::runtime_error("Player unavailable or dead");
            const auto elapsed=Now()-phaseStart;
            switch(phase) {
            case 0:
                Check("healthy-baseline",Engine::HealthAV() && scenes && !Defeat::IsDefeated(player) &&
                    DefeatRegistry::Snapshot().empty() && scenes->GetSceneForActor(player)==0 && Eligibility::IsEligible(player));
                initialHealth=Engine::AVOwnerOf(*player).GetActorValue(*Engine::HealthAV());
                Check("scripted-defeat",Defeat::DefeatActor(player,nullptr) && Defeat::GetDefeatCause(player)==DefeatCause::kScripted);
                Recovery::Hold(player); Next(); break;
            case 1:
                if(scenes->GetSceneForActor(player)==0 && elapsed<15000) break;
                Check("down-scene",scenes->GetSceneForActor(player)!=0);
                if(!Recovery::ScheduleAutomatic(player,3)) throw std::runtime_error("Could not schedule recovery"); Next(); break;
            case 2:
                if(Defeat::IsDefeated(player) && elapsed<15000) break;
                Check("automatic-recovery",!Defeat::IsDefeated(player)); Next(); break;
            case 3:
                if(scenes->GetSceneForActor(player)!=0 && elapsed<10000) break;
                if(!Defeat::DefeatActor(player,nullptr)) throw std::runtime_error("Could not begin leased defeat");
                Check("lease-ownership",Recovery::AcquireLease(player,"runtime-test",3) &&
                    !Recovery::AcquireLease(player,"competing-test",30) && !Recovery::ReleaseLease(player,"competing-test") &&
                    Recovery::GetLeaseHolder(player)=="runtime-test"); Next(); break;
            case 4:
                if(Defeat::IsDefeated(player) && elapsed<15000) break;
                Check("watchdog-recovery",!Defeat::IsDefeated(player)); Next(); break;
            case 5:
                if(scenes->GetSceneForActor(player)!=0 && elapsed<10000) break;
                {
                    auto& owner=Engine::AVOwnerOf(*player);
                    const auto health=owner.GetActorValue(*Engine::HealthAV());
                    // Use the exact five-argument entry already verified by the production
                    // hook (HookTrigger.md). CommonLib's overloaded virtual declarations
                    // selected the wrong slot in the initial probe and omitted a_ref.
                    using Damage = void(*)(RE::ActorValueOwner*,RE::ACTOR_VALUE_MODIFIER,const RE::ActorValueInfo&,float,RE::TESObjectREFR*);
                    REL::Relocation<Damage> damage{REL::ID{101587}};
                    damage(&owner,RE::ACTOR_VALUE_MODIFIER::kDamage,*Engine::HealthAV(),-std::max(health+100.0f,100.0f),nullptr);
                }
                Next(); break;
            case 6:
                if(!Defeat::IsDefeated(player) && elapsed<5000) break;
                Check("lethal-health-hook",!player->IsDead() && Defeat::GetDefeatCause(player)==DefeatCause::kHealth &&
                    DefeatContext::GetLastPlayerDefeatCause()==DefeatCause::kHealth && Engine::AVOwnerOf(*player).GetActorValue(*Engine::HealthAV())>0);
                Check("emergency-recovery",Defeat::ForceRecoverAll()==1 && !Defeat::IsDefeated(player)); Next(); break;
            case 7:
                if(elapsed<5000) break;
                Check("presentation-stays-cleared",!Defeat::IsDefeated(player) && scenes->GetSceneForActor(player)==0);
                Next(); break;
            case 8:
                if(!Acceptance::Tick(snapshot,player,scenes,directory,shadowsOnly,blackoutColdStart)) break;
                Cleanup(player); snapshot["state"]="passed"; finished=true; running=false; break;
            }
            Publish();
        } catch(const std::exception& error) {
            REX::ERROR("[OSFDefeat tests] {}",error.what());
            if(running) Cleanup(static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton()));
            running=false; finished=true;
            if(!directory.empty()) {
                snapshot["state"]="failed"; snapshot["error"]=error.what();
                try { Publish(); } catch(...) {}
            }
        }
    }
}
