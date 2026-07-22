#include "pch.h"

#include "Core/Persistence.h"

#include "Core/Capture.h"
#include "Core/Defeat.h"
#include "Core/DefeatRegistry.h"
#include "Core/Events.h"
#include "Engine/Engine.h"
#include "External/OSFPersistenceAPI.h"

#include "REX/W32/KERNEL32.h"

#include <type_traits>

namespace OSFDefeat::Persistence
{
    namespace
    {
        constexpr std::uint32_t kClientID = OSF_PERSISTENCE_FOURCC('O', 'S', 'F', 'D');
        constexpr std::uint32_t kRecDefeated = OSF_PERSISTENCE_FOURCC('D', 'F', 'T', '0');
        constexpr std::uint32_t kRecCaptive = OSF_PERSISTENCE_FOURCC('C', 'P', 'T', '0');
        constexpr std::uint32_t kRecCapture = OSF_PERSISTENCE_FOURCC('C', 'A', 'P', '0');
        constexpr std::uint32_t kDefeatedVersion = 1;
        constexpr std::uint32_t kCaptiveVersion = 1;
        constexpr std::uint32_t kCaptureVersion = 1;
        constexpr std::uint32_t kMaxLeaseOwner = 256;

        struct DefeatedRecord
        {
            FormID      formId{};
            float       registeredDay{};
            FormID      aggressor{};
            float       recoverAtDay{};
            float       leaseDeadlineDay{};
            std::string leaseOwner;
            DefeatCause cause{ DefeatCause::kNone };
        };

        struct CaptiveFactionRecord
        {
            FormID actorId{};
            FormID factionId{};
        };

        struct CaptureRecord
        {
            FormID captiveId{};
            FormID captorId{};
            float  capturedDay{};
        };

        struct LoadedState
        {
            std::vector<DefeatedRecord>       defeated;
            std::vector<CaptiveFactionRecord> captiveFactions;
            std::vector<CaptureRecord>        captures;
        };

        std::atomic_uint64_t g_loadGeneration{ 0 };
        std::uint8_t         g_clientContext{};
        std::atomic_bool     g_registered{ false };

        template <class T>
        bool WriteValue(OSFRecordWriterV1* a_writer, const T& a_value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            return a_writer && a_writer->WriteRecordData &&
                   a_writer->WriteRecordData(a_writer, &a_value, static_cast<std::uint32_t>(sizeof(T))) != 0;
        }

        bool WriteBytes(OSFRecordWriterV1* a_writer, const void* a_data, std::uint32_t a_length)
        {
            return a_length == 0 || (a_writer && a_writer->WriteRecordData &&
                   a_writer->WriteRecordData(a_writer, a_data, a_length) != 0);
        }

        template <class T>
        bool ReadValue(OSFRecordReaderV1* a_reader, std::uint32_t& a_remaining, T& a_value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            if (!a_reader || !a_reader->ReadRecordData || a_remaining < sizeof(T)) {
                return false;
            }
            if (a_reader->ReadRecordData(a_reader, &a_value, static_cast<std::uint32_t>(sizeof(T))) != sizeof(T)) {
                return false;
            }
            a_remaining -= static_cast<std::uint32_t>(sizeof(T));
            return true;
        }

        bool ReadBytes(OSFRecordReaderV1* a_reader, std::uint32_t& a_remaining, void* a_data, std::uint32_t a_length)
        {
            if (!a_reader || !a_reader->ReadRecordData || a_length > a_remaining) {
                return false;
            }
            if (a_length > 0 && a_reader->ReadRecordData(a_reader, a_data, a_length) != a_length) {
                return false;
            }
            a_remaining -= a_length;
            return true;
        }

        bool ResolveRequired(OSFRecordReaderV1* a_reader, FormID a_saved, FormID& a_resolved)
        {
            return a_saved != 0 && a_reader && a_reader->ResolveFormID &&
                   a_reader->ResolveFormID(a_reader, a_saved, &a_resolved) != 0;
        }

        FormID ResolveOptional(OSFRecordReaderV1* a_reader, FormID a_saved)
        {
            FormID resolved{};
            return a_saved != 0 && ResolveRequired(a_reader, a_saved, resolved) ? resolved : 0;
        }

        std::vector<DefeatedRecord> SnapshotDefeated()
        {
            std::vector<DefeatedRecord> records;
            const auto ids = DefeatRegistry::Snapshot();
            records.reserve(ids.size());
            for (const auto id : ids) {
                const auto data = DefeatRegistry::Get(id);
                if (!data) {
                    continue;
                }
                records.push_back({ id, data->registeredDay, data->aggressor,
                    data->recoverAtDay.load(std::memory_order_relaxed),
                    data->leaseDeadlineDay.load(std::memory_order_relaxed), data->CopyLeaseOwner(),
                    data->cause.load(std::memory_order_relaxed) });
            }
            return records;
        }

        bool SaveDefeated(OSFRecordWriterV1* a_writer, const std::vector<DefeatedRecord>& a_records)
        {
            if (a_records.empty()) {
                return true;
            }
            if (!a_writer->OpenRecord(a_writer, kRecDefeated, kDefeatedVersion)) {
                return false;
            }
            const auto count = static_cast<std::uint32_t>(a_records.size());
            if (!WriteValue(a_writer, count)) {
                return false;
            }
            for (const auto& record : a_records) {
                const auto ownerLength = static_cast<std::uint32_t>(
                    std::min<std::size_t>(record.leaseOwner.size(), kMaxLeaseOwner));
                const auto cause = static_cast<std::uint8_t>(record.cause);
                if (!WriteValue(a_writer, record.formId) || !WriteValue(a_writer, record.registeredDay) ||
                    !WriteValue(a_writer, record.aggressor) || !WriteValue(a_writer, record.recoverAtDay) ||
                    !WriteValue(a_writer, record.leaseDeadlineDay) || !WriteValue(a_writer, ownerLength) ||
                    !WriteBytes(a_writer, record.leaseOwner.data(), ownerLength) || !WriteValue(a_writer, cause)) {
                    return false;
                }
            }
            return true;
        }

        bool SaveCaptiveFactions(OSFRecordWriterV1* a_writer,
            const std::vector<std::pair<FormID, FormID>>& a_records)
        {
            if (a_records.empty()) {
                return true;
            }
            if (!a_writer->OpenRecord(a_writer, kRecCaptive, kCaptiveVersion)) {
                return false;
            }
            const auto count = static_cast<std::uint32_t>(a_records.size());
            if (!WriteValue(a_writer, count)) {
                return false;
            }
            for (const auto& [actorId, factionId] : a_records) {
                if (!WriteValue(a_writer, actorId) || !WriteValue(a_writer, factionId)) {
                    return false;
                }
            }
            return true;
        }

        bool SaveCaptures(OSFRecordWriterV1* a_writer,
            const std::vector<std::tuple<FormID, FormID, float>>& a_records)
        {
            if (a_records.empty()) {
                return true;
            }
            if (!a_writer->OpenRecord(a_writer, kRecCapture, kCaptureVersion)) {
                return false;
            }
            const auto count = static_cast<std::uint32_t>(a_records.size());
            if (!WriteValue(a_writer, count)) {
                return false;
            }
            for (const auto& [captiveId, captorId, capturedDay] : a_records) {
                if (!WriteValue(a_writer, captiveId) || !WriteValue(a_writer, captorId) ||
                    !WriteValue(a_writer, capturedDay)) {
                    return false;
                }
            }
            return true;
        }

        std::uint8_t OSF_PERSISTENCE_CALL Save(void*, OSFRecordWriterV1* a_writer)
        {
            try {
                if (!a_writer || a_writer->size < sizeof(OSFRecordWriterV1) ||
                    !a_writer->OpenRecord || !a_writer->WriteRecordData) {
                    REX::ERROR("[OSFDefeat] OSF persistence supplied an invalid writer");
                    return 0;
                }
                const auto defeated = SnapshotDefeated();
                const auto captiveFactions = Engine::SnapshotCaptiveFactions();
                const auto captures = Capture::Snapshot();
                if (!SaveDefeated(a_writer, defeated) || !SaveCaptiveFactions(a_writer, captiveFactions) ||
                    !SaveCaptures(a_writer, captures)) {
                    REX::ERROR("[OSFDefeat] failed writing OSF persistence records");
                    return 0;
                }
                REX::INFO("[OSFDefeat] saved state: {} defeated, {} captive-faction, {} capture record(s)",
                    defeated.size(), captiveFactions.size(), captures.size());
                return 1;
            } catch (const std::exception& e) {
                REX::ERROR("[OSFDefeat] persistence save failed: {}", e.what());
            } catch (...) {
                REX::ERROR("[OSFDefeat] persistence save failed with an unknown exception");
            }
            return 0;
        }

        bool ReadDefeated(OSFRecordReaderV1* a_reader, std::uint32_t a_length, LoadedState& a_state)
        {
            std::uint32_t count{};
            if (!ReadValue(a_reader, a_length, count)) {
                return false;
            }
            constexpr std::uint32_t kMinimumEntrySize =
                sizeof(FormID) * 2 + sizeof(float) * 3 + sizeof(std::uint32_t) + sizeof(std::uint8_t);
            if (count > a_length / kMinimumEntrySize) {
                return false;
            }
            a_state.defeated.reserve(a_state.defeated.size() + count);
            for (std::uint32_t i = 0; i < count; ++i) {
                DefeatedRecord record;
                std::uint32_t ownerLength{};
                std::uint8_t cause{};
                if (!ReadValue(a_reader, a_length, record.formId) ||
                    !ReadValue(a_reader, a_length, record.registeredDay) ||
                    !ReadValue(a_reader, a_length, record.aggressor) ||
                    !ReadValue(a_reader, a_length, record.recoverAtDay) ||
                    !ReadValue(a_reader, a_length, record.leaseDeadlineDay) ||
                    !ReadValue(a_reader, a_length, ownerLength) ||
                    ownerLength > kMaxLeaseOwner || ownerLength > a_length) {
                    return false;
                }
                record.leaseOwner.resize(ownerLength);
                if (!ReadBytes(a_reader, a_length, record.leaseOwner.data(), ownerLength) ||
                    !ReadValue(a_reader, a_length, cause)) {
                    return false;
                }
                if (cause > static_cast<std::uint8_t>(DefeatCause::kScripted)) {
                    cause = static_cast<std::uint8_t>(DefeatCause::kNone);
                }
                record.cause = static_cast<DefeatCause>(cause);
                FormID resolvedVictim{};
                if (!ResolveRequired(a_reader, record.formId, resolvedVictim)) {
                    REX::WARN("[OSFDefeat] persistence dropped unresolved defeated form {:08X}", record.formId);
                    continue;
                }
                record.formId = resolvedVictim;
                record.aggressor = ResolveOptional(a_reader, record.aggressor);
                a_state.defeated.push_back(std::move(record));
            }
            return a_length == 0;
        }

        bool ReadCaptiveFactions(OSFRecordReaderV1* a_reader, std::uint32_t a_length, LoadedState& a_state)
        {
            std::uint32_t count{};
            if (!ReadValue(a_reader, a_length, count) || count > a_length / (sizeof(FormID) * 2)) {
                return false;
            }
            a_state.captiveFactions.reserve(a_state.captiveFactions.size() + count);
            for (std::uint32_t i = 0; i < count; ++i) {
                CaptiveFactionRecord record;
                FormID savedActor{}, savedFaction{};
                if (!ReadValue(a_reader, a_length, savedActor) || !ReadValue(a_reader, a_length, savedFaction)) {
                    return false;
                }
                if (ResolveRequired(a_reader, savedActor, record.actorId) &&
                    ResolveRequired(a_reader, savedFaction, record.factionId)) {
                    a_state.captiveFactions.push_back(record);
                }
            }
            return a_length == 0;
        }

        bool ReadCaptures(OSFRecordReaderV1* a_reader, std::uint32_t a_length, LoadedState& a_state)
        {
            std::uint32_t count{};
            constexpr std::uint32_t kEntrySize = sizeof(FormID) * 2 + sizeof(float);
            if (!ReadValue(a_reader, a_length, count) || count > a_length / kEntrySize) {
                return false;
            }
            a_state.captures.reserve(a_state.captures.size() + count);
            for (std::uint32_t i = 0; i < count; ++i) {
                CaptureRecord record;
                FormID savedCaptive{}, savedCaptor{};
                if (!ReadValue(a_reader, a_length, savedCaptive) || !ReadValue(a_reader, a_length, savedCaptor) ||
                    !ReadValue(a_reader, a_length, record.capturedDay)) {
                    return false;
                }
                if (!ResolveRequired(a_reader, savedCaptive, record.captiveId)) {
                    continue;
                }
                record.captorId = ResolveOptional(a_reader, savedCaptor);
                a_state.captures.push_back(record);
            }
            return a_length == 0;
        }

        void ApplyLoadedState(LoadedState a_state, std::uint64_t a_generation)
        {
            if (g_loadGeneration.load(std::memory_order_acquire) != a_generation) {
                REX::WARN("[OSFDefeat] discarded stale deferred persistence load");
                return;
            }
            for (auto& record : a_state.defeated) {
                Defeat::RestoreFromSave(record.formId, record.registeredDay, record.aggressor,
                    record.recoverAtDay, record.leaseDeadlineDay, std::move(record.leaseOwner), record.cause);
            }
            for (const auto& record : a_state.captiveFactions) {
                Engine::RestoreCaptiveFaction(record.actorId, record.factionId);
            }
            for (const auto& record : a_state.captures) {
                Capture::RestoreCapture(record.captiveId, record.captorId, record.capturedDay);
            }
            REX::INFO("[OSFDefeat] loaded state: {} defeated, {} captive-faction, {} capture record(s)",
                a_state.defeated.size(), a_state.captiveFactions.size(), a_state.captures.size());
        }

        std::uint8_t OSF_PERSISTENCE_CALL Load(void*, OSFRecordReaderV1* a_reader)
        {
            try {
                if (!a_reader || a_reader->size < sizeof(OSFRecordReaderV1) ||
                    !a_reader->GetNextRecordInfo || !a_reader->ReadRecordData || !a_reader->ResolveFormID) {
                    REX::ERROR("[OSFDefeat] OSF persistence supplied an invalid reader");
                    return 0;
                }
                LoadedState state;
                std::uint32_t type{}, version{}, length{};
                while (a_reader->GetNextRecordInfo(a_reader, &type, &version, &length)) {
                    bool ok = true;
                    if (type == kRecDefeated && version == kDefeatedVersion) {
                        ok = ReadDefeated(a_reader, length, state);
                    } else if (type == kRecCaptive && version == kCaptiveVersion) {
                        ok = ReadCaptiveFactions(a_reader, length, state);
                    } else if (type == kRecCapture && version == kCaptureVersion) {
                        ok = ReadCaptures(a_reader, length, state);
                    }
                    if (!ok) {
                        REX::ERROR("[OSFDefeat] corrupt persistence record type {:#010x} version {}", type, version);
                        return 0;
                    }
                }
                const auto* const tasks = SFSE::GetTaskInterface();
                if (!tasks) {
                    REX::ERROR("[OSFDefeat] task interface unavailable; persistence load not applied");
                    return 0;
                }
                const auto generation = g_loadGeneration.load(std::memory_order_acquire);
                tasks->AddTask([state = std::move(state), generation]() mutable {
                    ApplyLoadedState(std::move(state), generation);
                });
                return 1;
            } catch (const std::exception& e) {
                REX::ERROR("[OSFDefeat] persistence load failed: {}", e.what());
            } catch (...) {
                REX::ERROR("[OSFDefeat] persistence load failed with an unknown exception");
            }
            return 0;
        }

        void OSF_PERSISTENCE_CALL Revert(void*)
        {
            g_loadGeneration.fetch_add(1, std::memory_order_acq_rel);
            DefeatRegistry::Clear();
            Events::Clear();
            Engine::ClearCaptiveFactions();
            Capture::Clear();
            REX::INFO("[OSFDefeat] reverted defeat state");
        }
    }

    void Register()
    {
        if (g_registered.load(std::memory_order_acquire)) {
            return;
        }
        const auto module = REX::W32::GetModuleHandleW(L"OSF Animation.dll");
        if (!module) {
            REX::ERROR("[OSFDefeat] OSF Animation module unavailable; persistence disabled");
            return;
        }
        const auto request = reinterpret_cast<OSF_RequestPersistenceAPI_t>(
            REX::W32::GetProcAddress(module, "OSF_RequestPersistenceAPI"));
        if (!request) {
            REX::ERROR("[OSFDefeat] OSF Animation persistence export unavailable; update OSF Animation");
            return;
        }
        const auto* const api = request(OSF_PERSISTENCE_API_VERSION);
        if (!api || api->size < sizeof(OSFPersistenceAPI) || !api->IsReady || !api->RegisterClient ||
            !api->GetInterfaceVersion || !api->GetPluginVersion || !api->IsReady(api)) {
            REX::ERROR("[OSFDefeat] OSF Animation persistence API 1.0 unavailable or not ready");
            return;
        }
        OSFPersistenceClientV1 client{};
        client.size = sizeof(client);
        client.clientID = kClientID;
        client.clientName = "OSF Defeat";
        client.context = &g_clientContext;
        client.save = Save;
        client.load = Load;
        client.revert = Revert;
        if (!api->RegisterClient(api, &client)) {
            REX::ERROR("[OSFDefeat] failed to register OSFD persistence client");
            return;
        }
        g_registered.store(true, std::memory_order_release);
        std::uint32_t major{}, minor{}, patch{};
        api->GetPluginVersion(api, &major, &minor, &patch);
        REX::INFO("[OSFDefeat] OSF persistence registered (ABI {:#x}, OSF Animation {}.{}.{})",
            api->GetInterfaceVersion(api), major, minor, patch);
    }
}
