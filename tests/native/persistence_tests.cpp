#include "Test.h"
#include "Core/Persistence.h"
#include "External/OSFPersistenceAPI.h"
using namespace Test;
namespace {
    constexpr auto DFT=OSF_PERSISTENCE_FOURCC('D','F','T','0');
    constexpr auto CPT=OSF_PERSISTENCE_FOURCC('C','P','T','0');
    constexpr auto CAP=OSF_PERSISTENCE_FOURCC('C','A','P','0');
    constexpr auto PCF=OSF_PERSISTENCE_FOURCC('P','C','F','0');
    OSFPersistenceClientV1 client{};
    bool ready=true;
    OSFPersistenceAPI api{sizeof(api), nullptr,
        [](const OSFPersistenceAPI*) -> std::uint8_t { return ready; },
        [](const OSFPersistenceAPI*,const OSFPersistenceClientV1* value) -> std::uint8_t { client=*value; return 1; },
        nullptr, [](const OSFPersistenceAPI*) -> std::uint32_t { return OSF_PERSISTENCE_API_VERSION; },
        [](const OSFPersistenceAPI*,std::uint32_t* a,std::uint32_t* b,std::uint32_t* c) { *a=1; *b=0; *c=0; }};
    const OSFPersistenceAPI* OSF_PERSISTENCE_CALL Request(std::uint32_t version) { return version==OSF_PERSISTENCE_API_VERSION ? &api : nullptr; }
    void Connect() {
        client={}; ready=true; TestEngine::persistenceExport=reinterpret_cast<void*>(&Request);
        Persistence::Register(); CHECK(client.save && client.load && client.revert);
        CHECK(client.clientID==OSF_PERSISTENCE_FOURCC('O','S','F','D'));
    }
    struct Record { std::uint32_t type{}, version{}; std::vector<std::uint8_t> bytes; };
    struct Archive {
        std::vector<Record> records;
        std::size_t index=0, offset=0;
        bool openWorks=true, writeWorks=true, shortRead=false;
        std::map<FormID,FormID> remap;
        std::set<FormID> missing;
        OSFRecordWriterV1 writer{sizeof(writer),this,
            [](OSFRecordWriterV1* w,std::uint32_t type,std::uint32_t version) -> std::uint8_t {
                auto& a=*static_cast<Archive*>(w->context); if(!a.openWorks) return 0;
                a.records.push_back({type,version,{}}); return 1;
            },
            [](OSFRecordWriterV1* w,const void* data,std::uint32_t length) -> std::uint8_t {
                auto& a=*static_cast<Archive*>(w->context); if(!a.writeWorks) return 0;
                const auto* bytes=static_cast<const std::uint8_t*>(data);
                a.records.back().bytes.insert(a.records.back().bytes.end(),bytes,bytes+length); return 1;
            }};
        OSFRecordReaderV1 reader{sizeof(reader),this,
            [](OSFRecordReaderV1* r,std::uint32_t* type,std::uint32_t* version,std::uint32_t* length) -> std::uint8_t {
                auto& a=*static_cast<Archive*>(r->context); if(a.index==a.records.size()) return 0;
                const auto& record=a.records[a.index++]; a.offset=0;
                *type=record.type; *version=record.version; *length=static_cast<std::uint32_t>(record.bytes.size()); return 1;
            },
            [](OSFRecordReaderV1* r,void* data,std::uint32_t length) -> std::uint32_t {
                auto& a=*static_cast<Archive*>(r->context); const auto& record=a.records[a.index-1];
                const auto count=std::min<std::size_t>(length,record.bytes.size()-a.offset);
                if(a.shortRead) return 0;
                std::memcpy(data,record.bytes.data()+a.offset,count); a.offset+=count; return static_cast<std::uint32_t>(count);
            },
            [](OSFRecordReaderV1* r,FormID old,FormID* resolved) -> std::uint8_t {
                auto& a=*static_cast<Archive*>(r->context); if(a.missing.contains(old)) return 0;
                *resolved=a.remap.contains(old) ? a.remap.at(old) : old; return 1;
            }};
        bool Save() { return client.save(client.context,&writer)!=0; }
        bool Load() { index=offset=0; return client.load(client.context,&reader)!=0; }
    };
    void Drain() { Persistence::ApplyPending(); RE::BSService::TaskQueue::GetSingleton()->Drain(); }
    void Revert() { client.revert(client.context); Drain(); }
    void Seed(RE::Actor& actor) {
        Add(actor); CHECK(Capture::CaptureActor(&actor,nullptr)); CHECK(Recovery::AcquireLease(&actor,"consumer",12));
        Defeat::SuspendAI(&actor); auto data=DefeatRegistry::Get(actor.id);
        data->priorState={true,false,true,true,true,true,true,RE::ACTOR_LIFE_STATE::kBleedOut};
    }
    template<class T> void Set(Record& r,std::size_t offset,T value) {
        CHECK(offset+sizeof(value)<=r.bytes.size()); std::memcpy(r.bytes.data()+offset,&value,sizeof(value));
    }
}

TEST(registration_refuses_missing_or_unready_host) {
    client={}; Persistence::Register(); CHECK(!client.save);
    TestEngine::persistenceExport=reinterpret_cast<void*>(&Request); ready=false;
    Persistence::Register(); CHECK(!client.save); ready=true;
    TestEngine::moduleAvailable=false; Persistence::Register(); CHECK(!client.save);
    TestEngine::moduleAvailable=true; Connect();
}
TEST(empty_save_emits_no_records_and_invalid_interfaces_fail) {
    Connect(); Archive a; CHECK(a.Save()); CHECK(a.records.empty());
    CHECK(!client.save(client.context,nullptr)); CHECK(!client.load(client.context,nullptr));
    a.writer.size=0; CHECK(!a.Save()); a.writer.size=sizeof(a.writer); a.writer.OpenRecord=nullptr; CHECK(!a.Save());
    a.reader.size=0; CHECK(!a.Load()); a.reader.size=sizeof(a.reader); a.reader.ResolveFormID=nullptr; CHECK(!a.Load());
}
TEST(all_record_types_round_trip_and_engine_application_is_deferred) {
    Connect(); RE::Actor actor(1); Seed(actor); Archive a; CHECK(a.Save()); CHECK(a.records.size()==4);
    CHECK(a.records[0].type==DFT && a.records[0].version==2); CHECK(a.records[1].type==CPT && a.records[1].version==1);
    CHECK(a.records[2].type==CAP && a.records[2].version==2); CHECK(a.records[3].type==PCF && a.records[3].version==1);
    const float deadline=DefeatRegistry::Get(1)->leaseExpiresAtDay; Revert(); CHECK(DefeatRegistry::Snapshot().empty());
    CHECK(a.Load()); CHECK(DefeatRegistry::Snapshot().empty()); Drain();
    auto data=DefeatRegistry::Get(1); CHECK(data && data->CopyLeaseHolderId()=="consumer" && data->leaseExpiresAtDay==deadline);
    CHECK(data->priorState.valid && !data->priorState.aiEnabled && data->priorState.noBleedoutRecovery);
    CHECK(data->priorState.ghost && data->priorState.actorEssential && data->priorState.baseEssential && data->priorState.canSpeakToEssentialDown);
    CHECK(data->priorState.lifeState==RE::ACTOR_LIFE_STATE::kBleedOut); CHECK(Capture::IsCaptive(&actor));
    CHECK(Engine::HasCaptiveFactionApplied(actor)); CHECK(Defeat::IsAISuspended(&actor));
}
TEST(legacy_v1_defeat_and_capture_get_documented_defaults) {
    Connect(); RE::Actor actor(1); Seed(actor); Archive a; CHECK(a.Save());
    a.records[0].version=1; a.records[0].bytes.resize(a.records[0].bytes.size()-2);
    a.records[2].version=1; a.records[2].bytes.pop_back(); Revert(); CHECK(a.Load()); Drain();
    const auto data=DefeatRegistry::Get(1); CHECK(data->priorState.valid && data->priorState.aiEnabled && !data->priorState.ghost);
    CHECK(data->priorState.lifeState==RE::ACTOR_LIFE_STATE::kAlive); CHECK(Capture::Snapshot()[0].removeOwnedFactionOnRelease);
}
TEST(every_truncated_record_is_rejected_without_partial_application) {
    Connect(); RE::Actor actor(1); Seed(actor); Archive original; CHECK(original.Save()); Revert();
    for(std::size_t record=0;record<original.records.size();++record) {
        for(std::size_t length=0;length<original.records[record].bytes.size();++length) {
            Archive a; a.records=original.records; a.records[record].bytes.resize(length);
            CHECK(!a.Load()); CHECK(RE::BSService::TaskQueue::GetSingleton()->pending.empty());
            CHECK(DefeatRegistry::Snapshot().empty() && Capture::Snapshot().empty() && Engine::SnapshotCaptiveFactions().empty());
        }
    }
}
TEST(corrupt_counts_trailing_bytes_and_invalid_flags_fail_atomically) {
    Connect(); RE::Actor actor(1); Seed(actor); Archive original; CHECK(original.Save()); Revert();
    for(std::size_t i=0;i<original.records.size();++i) {
        Archive a; a.records=original.records; Set(a.records[i],0,std::uint32_t{0xFFFFFFFF}); CHECK(!a.Load());
        a.records=original.records; a.records[i].bytes.push_back(0); CHECK(!a.Load());
    }
    Archive a; a.records=original.records; a.records[0].bytes.back()=0xFF; CHECK(!a.Load());
    a.records=original.records; a.records[0].bytes[a.records[0].bytes.size()-2]=0; CHECK(!a.Load());
    a.records=original.records; a.records[2].bytes.back()=2; CHECK(!a.Load());
    a.records=original.records; a.records[3].bytes.back()=2; CHECK(!a.Load());
    a.records=original.records; Set(a.records[0],24,std::uint32_t{257}); CHECK(!a.Load());
    CHECK(DefeatRegistry::Snapshot().empty());
}
TEST(nonfinite_saved_times_rejected_and_unknown_cause_normalized) {
    Connect(); RE::Actor actor(1); Seed(actor); Archive original; CHECK(original.Save()); Revert();
    for(auto offset : {8u,16u,20u}) {
        Archive a; a.records=original.records; Set(a.records[0],offset,std::numeric_limits<float>::quiet_NaN()); CHECK(!a.Load());
    }
    Archive a; a.records=original.records; Set(a.records[2],12,std::numeric_limits<float>::infinity()); CHECK(!a.Load());
    a.records=original.records; a.records[0].bytes[a.records[0].bytes.size()-3]=255; CHECK(a.Load()); Drain();
    CHECK(Defeat::GetDefeatCause(&actor)==DefeatCause::kNone);
}
TEST(form_remapping_and_missing_forms_are_handled_by_record_role) {
    Connect(); RE::Actor actor(1), aggressor(2); Add(aggressor); Seed(actor); DefeatRegistry::Get(1)->aggressor=2;
    Archive a; CHECK(a.Save()); Revert(); a.remap[1]=101; a.missing.insert(2); CHECK(a.Load()); Drain();
    CHECK(!DefeatRegistry::Contains(1)); CHECK(DefeatRegistry::Get(101)->aggressor==0);
    CHECK(Capture::Snapshot()[0].captiveId==101);
    Revert(); a.missing.insert(1); CHECK(a.Load()); Drain(); CHECK(DefeatRegistry::Snapshot().empty()); CHECK(Capture::Snapshot().empty());
}
TEST(unknown_record_and_future_version_are_skipped) {
    Connect(); RE::Actor actor(1); Seed(actor); Archive a; CHECK(a.Save()); Revert();
    a.records.insert(a.records.begin(),Record{123,99,{0,1,2}}); CHECK(a.Load()); Drain(); CHECK(DefeatRegistry::Contains(1));
    Revert(); a.records[1].version=99; CHECK(a.Load()); Drain(); CHECK(!DefeatRegistry::Contains(1));
}
TEST(write_failures_short_reads_fail_and_load_survives_disabled_queue) {
    Connect(); RE::Actor actor(1); Seed(actor); Archive a; a.openWorks=false; CHECK(!a.Save());
    a.openWorks=true; a.writeWorks=false; CHECK(!a.Save()); a.records.clear(); a.writeWorks=true; CHECK(a.Save());
    Revert(); a.shortRead=true; CHECK(!a.Load()); a.shortRead=false; TestEngine::queueEnabled=0;
    CHECK(a.Load()); CHECK(DefeatRegistry::Snapshot().empty());
    TestEngine::queueEnabled=1; Drain(); CHECK(DefeatRegistry::Contains(1));
    CHECK(Capture::IsCaptive(&actor)); CHECK(Defeat::IsAISuspended(&actor));
}
TEST(revert_invalidates_pending_load_and_resets_session_state) {
    Connect(); RE::Actor actor(1); Seed(actor); Archive a; CHECK(a.Save()); Revert();
    CHECK(a.Load()); const auto epoch=RuntimeMutation::CurrentEpoch(); client.revert(client.context);
    CHECK(!RuntimeMutation::IsCurrent(epoch)); Drain(); CHECK(!DefeatRegistry::Contains(1));
    Eligibility::SetEnabled(false); DefeatContext::SetPlayerContext(nullptr,Consequence::PlayerConsequence{});
    auto r=std::make_shared<RE::BSScript::Object>(); Events::RegisterPlayerDefeated(r,"event");
    Revert(); CHECK(Eligibility::IsEnabled()); CHECK(DefeatContext::GetLastPlayerConsequenceId().empty());
    Events::BroadcastPlayerDefeated(); CHECK(vm.calls.empty()); CHECK(resetCalls>=1);
}
TEST(inline_queue_cannot_apply_load_before_main_update_and_long_lease_id_round_trip) {
    Connect(); RE::Actor actor(1); Seed(actor); DefeatRegistry::Get(1)->SetLeaseHolderId(std::string(400,'x'));
    Archive a; CHECK(a.Save()); Revert(); RE::BSService::TaskQueue::GetSingleton()->inlineTasks=true;
    CHECK(a.Load()); CHECK(!DefeatRegistry::Contains(1)); Drain();
    CHECK(DefeatRegistry::Get(1)->CopyLeaseHolderId()==std::string(256,'x'));
}

TEST(revert_with_disabled_queue_is_applied_before_latest_load_only) {
    Connect(); RE::Actor actor(1); Seed(actor); Archive a; CHECK(a.Save());
    TestEngine::queueEnabled=0; client.revert(client.context);
    CHECK(DefeatRegistry::Contains(1)); CHECK(a.Load());
    client.revert(client.context); TestEngine::queueEnabled=1; Drain();
    CHECK(DefeatRegistry::Snapshot().empty()); CHECK(Capture::Snapshot().empty());
    CHECK(Engine::SnapshotCaptiveFactions().empty()); CHECK(Defeat::SnapshotAISuspensions().empty());
    CHECK(a.Load()); Drain(); CHECK(DefeatRegistry::Contains(1));
}
