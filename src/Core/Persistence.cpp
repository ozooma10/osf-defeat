#include "pch.h"

#include "Core/Persistence.h"

#include "Core/Capture.h"
#include "Core/Consequence.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/DefeatRegistry.h"
#include "Core/Events.h"
#include "Core/PlayerDefeat.h"
#include "Core/Presentation.h"
#include "Core/RuntimeMutation.h"
#include "Core/Eligibility.h"
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
        constexpr std::uint32_t kRecOwnedCaptiveFaction = OSF_PERSISTENCE_FOURCC('C', 'P', 'T', '0');
        constexpr std::uint32_t kRecCaptureRelationship = OSF_PERSISTENCE_FOURCC('C', 'A', 'P', '0');
        constexpr std::uint32_t kRecAISuspension = OSF_PERSISTENCE_FOURCC('P', 'C', 'F', '0');
        constexpr std::uint32_t kDefeatedVersion = 2;
        constexpr std::uint32_t kOwnedCaptiveFactionVersion = 1;
        constexpr std::uint32_t kCaptureRelationshipVersion = 2;
        constexpr std::uint32_t kAISuspensionVersion = 1;
        constexpr std::uint32_t kMaxLeaseHolderIdLength = 256;

        struct DefeatedRecord
        {
            FormID      formId{};
            float       registeredDay{};
            FormID      aggressor{};
            float       autoRecoverAtDay{};
            float       leaseExpiresAtDay{};
            std::string leaseHolderId;
            DefeatCause cause{ DefeatCause::kNone };
            ActorStateSnapshot priorState;
        };

        struct OwnedCaptiveFactionRecord
        {
            FormID actorId{};
            FormID factionId{};
        };

        struct PersistedCaptureRelationship
        {
            FormID captiveId{};
            FormID captorId{};
            float  capturedDay{};
            bool   removeOwnedFactionOnRelease{ true };  // v1 compatibility: capture historically removed it
        };

        struct AISuspensionRecord
        {
            FormID actorId{};
            bool wasAIEnabled{ true };
        };

        struct LoadedState
        {
            std::vector<DefeatedRecord>       defeated;
            std::vector<OwnedCaptiveFactionRecord> ownedCaptiveFactions;
            std::vector<PersistedCaptureRelationship> captureRelationships;
            std::vector<AISuspensionRecord> aiSuspensions;
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
                    data->autoRecoverAtDay.load(std::memory_order_relaxed),
                    data->leaseExpiresAtDay.load(std::memory_order_relaxed), data->CopyLeaseHolderId(),
                    data->cause.load(std::memory_order_relaxed), data->priorState });
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
                const auto holderLength = static_cast<std::uint32_t>(
                    std::min<std::size_t>(record.leaseHolderId.size(), kMaxLeaseHolderIdLength));
                const auto cause = static_cast<std::uint8_t>(record.cause);
                std::uint8_t priorFlags = 0;
                priorFlags |= record.priorState.valid ? 1u << 0 : 0;
                priorFlags |= record.priorState.aiEnabled ? 1u << 1 : 0;
                priorFlags |= record.priorState.noBleedoutRecovery ? 1u << 2 : 0;
                priorFlags |= record.priorState.canSpeakToEssentialDown ? 1u << 3 : 0;
                priorFlags |= record.priorState.ghost ? 1u << 4 : 0;
                priorFlags |= record.priorState.actorEssential ? 1u << 5 : 0;
                priorFlags |= record.priorState.baseEssential ? 1u << 6 : 0;
                const auto lifeState = static_cast<std::uint8_t>(record.priorState.lifeState);
                if (!WriteValue(a_writer, record.formId) || !WriteValue(a_writer, record.registeredDay) ||
                    !WriteValue(a_writer, record.aggressor) || !WriteValue(a_writer, record.autoRecoverAtDay) ||
                    !WriteValue(a_writer, record.leaseExpiresAtDay) || !WriteValue(a_writer, holderLength) ||
                    !WriteBytes(a_writer, record.leaseHolderId.data(), holderLength) || !WriteValue(a_writer, cause) ||
                    !WriteValue(a_writer, priorFlags) || !WriteValue(a_writer, lifeState)) {
                    return false;
                }
            }
            return true;
        }

        bool SaveOwnedCaptiveFactions(OSFRecordWriterV1* a_writer,
            const std::vector<std::pair<FormID, FormID>>& a_records)
        {
            if (a_records.empty()) {
                return true;
            }
            if (!a_writer->OpenRecord(a_writer, kRecOwnedCaptiveFaction, kOwnedCaptiveFactionVersion)) {
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

        bool SaveCaptureRelationships(OSFRecordWriterV1* a_writer,
            const std::vector<Capture::CaptureRelationshipRecord>& a_records)
        {
            if (a_records.empty()) {
                return true;
            }
            if (!a_writer->OpenRecord(a_writer, kRecCaptureRelationship, kCaptureRelationshipVersion)) {
                return false;
            }
            const auto count = static_cast<std::uint32_t>(a_records.size());
            if (!WriteValue(a_writer, count)) {
                return false;
            }
            for (const auto& record : a_records) {
                const std::uint8_t removeFaction = record.removeOwnedFactionOnRelease ? 1 : 0;
                if (!WriteValue(a_writer, record.captiveId) || !WriteValue(a_writer, record.captorId) ||
                    !WriteValue(a_writer, record.capturedDay) || !WriteValue(a_writer, removeFaction)) {
                    return false;
                }
            }
            return true;
        }

        bool SaveAISuspensions(OSFRecordWriterV1* a_writer, const std::vector<std::pair<FormID, bool>>& a_records)
        {
            if (a_records.empty()) {
                return true;
            }
            if (!a_writer->OpenRecord(a_writer, kRecAISuspension, kAISuspensionVersion)) {
                return false;
            }
            const auto count = static_cast<std::uint32_t>(a_records.size());
            if (!WriteValue(a_writer, count)) {
                return false;
            }
            for (const auto& [actorId, wasAIEnabled] : a_records) {
                const std::uint8_t enabled = wasAIEnabled ? 1 : 0;
                if (!WriteValue(a_writer, actorId) || !WriteValue(a_writer, enabled)) {
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
                std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
                const auto defeated = SnapshotDefeated();
                const auto ownedCaptiveFactions = Engine::SnapshotCaptiveFactions();
                const auto captureRelationships = Capture::Snapshot();
                const auto aiSuspensions = Defeat::SnapshotAISuspensions();
                if (!SaveDefeated(a_writer, defeated) || !SaveOwnedCaptiveFactions(a_writer, ownedCaptiveFactions) ||
                    !SaveCaptureRelationships(a_writer, captureRelationships) || !SaveAISuspensions(a_writer, aiSuspensions)) {
                    REX::ERROR("[OSFDefeat] failed writing OSF persistence records");
                    return 0;
                }
                REX::INFO("[OSFDefeat] saved state: {} active defeats, {} owned captive-faction memberships, {} capture relationships, {} AI suspensions",
                    defeated.size(), ownedCaptiveFactions.size(), captureRelationships.size(), aiSuspensions.size());
                return 1;
            } catch (const std::exception& e) {
                REX::ERROR("[OSFDefeat] persistence save failed: {}", e.what());
            } catch (...) {
                REX::ERROR("[OSFDefeat] persistence save failed with an unknown exception");
            }
            return 0;
        }

        bool ReadDefeated(OSFRecordReaderV1* a_reader, std::uint32_t a_version, std::uint32_t a_length, LoadedState& a_state)
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
                std::uint32_t holderLength{};
                std::uint8_t cause{};
                if (!ReadValue(a_reader, a_length, record.formId) ||
                    !ReadValue(a_reader, a_length, record.registeredDay) ||
                    !ReadValue(a_reader, a_length, record.aggressor) ||
                    !ReadValue(a_reader, a_length, record.autoRecoverAtDay) ||
                    !ReadValue(a_reader, a_length, record.leaseExpiresAtDay) ||
                    !ReadValue(a_reader, a_length, holderLength) ||
                    holderLength > kMaxLeaseHolderIdLength || holderLength > a_length) {
                    return false;
                }
                record.leaseHolderId.resize(holderLength);
                if (!ReadBytes(a_reader, a_length, record.leaseHolderId.data(), holderLength) ||
                    !ReadValue(a_reader, a_length, cause)) {
                    return false;
                }
                if (a_version >= 2) {
                    std::uint8_t priorFlags{};
                    std::uint8_t lifeState{};
                    if (!ReadValue(a_reader, a_length, priorFlags) || !ReadValue(a_reader, a_length, lifeState) ||
                        lifeState > 0x0F) {
                        return false;
                    }
                    record.priorState.valid = (priorFlags & (1u << 0)) != 0;
                    record.priorState.aiEnabled = (priorFlags & (1u << 1)) != 0;
                    record.priorState.noBleedoutRecovery = (priorFlags & (1u << 2)) != 0;
                    record.priorState.canSpeakToEssentialDown = (priorFlags & (1u << 3)) != 0;
                    record.priorState.ghost = (priorFlags & (1u << 4)) != 0;
                    record.priorState.actorEssential = (priorFlags & (1u << 5)) != 0;
                    record.priorState.baseEssential = (priorFlags & (1u << 6)) != 0;
                    record.priorState.lifeState = static_cast<RE::ACTOR_LIFE_STATE>(lifeState);
                    if (!record.priorState.valid) {
                        return false;
                    }
                } else {
                    // Version 1 did not persist ownership snapshots. Preserve its documented legacy
                    // recovery behavior (alive, AI on, OSF flags off) rather than snapshotting the
                    // already-down state from the game save and restoring that forever.
                    record.priorState.valid = true;
                }
                if (!std::isfinite(record.registeredDay) || !std::isfinite(record.autoRecoverAtDay) ||
                    !std::isfinite(record.leaseExpiresAtDay)) {
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

        bool ReadOwnedCaptiveFactions(OSFRecordReaderV1* a_reader, std::uint32_t a_length, LoadedState& a_state)
        {
            std::uint32_t count{};
            if (!ReadValue(a_reader, a_length, count) || count > a_length / (sizeof(FormID) * 2)) {
                return false;
            }
            a_state.ownedCaptiveFactions.reserve(a_state.ownedCaptiveFactions.size() + count);
            for (std::uint32_t i = 0; i < count; ++i) {
                OwnedCaptiveFactionRecord record;
                FormID savedActor{}, savedFaction{};
                if (!ReadValue(a_reader, a_length, savedActor) || !ReadValue(a_reader, a_length, savedFaction)) {
                    return false;
                }
                if (ResolveRequired(a_reader, savedActor, record.actorId) &&
                    ResolveRequired(a_reader, savedFaction, record.factionId)) {
                    a_state.ownedCaptiveFactions.push_back(record);
                }
            }
            return a_length == 0;
        }

        bool ReadCaptureRelationships(OSFRecordReaderV1* a_reader, std::uint32_t a_version, std::uint32_t a_length,
            LoadedState& a_state)
        {
            std::uint32_t count{};
            constexpr std::uint32_t kEntrySize = sizeof(FormID) * 2 + sizeof(float);
            if (!ReadValue(a_reader, a_length, count) || count > a_length / kEntrySize) {
                return false;
            }
            a_state.captureRelationships.reserve(a_state.captureRelationships.size() + count);
            for (std::uint32_t i = 0; i < count; ++i) {
                PersistedCaptureRelationship record;
                FormID savedCaptive{}, savedCaptor{};
                if (!ReadValue(a_reader, a_length, savedCaptive) || !ReadValue(a_reader, a_length, savedCaptor) ||
                    !ReadValue(a_reader, a_length, record.capturedDay)) {
                    return false;
                }
                if (a_version >= 2) {
                    std::uint8_t removeFaction{};
                    if (!ReadValue(a_reader, a_length, removeFaction) || removeFaction > 1) {
                        return false;
                    }
                    record.removeOwnedFactionOnRelease = removeFaction != 0;
                }
                if (!std::isfinite(record.capturedDay)) {
                    return false;
                }
                if (!ResolveRequired(a_reader, savedCaptive, record.captiveId)) {
                    continue;
                }
                record.captorId = ResolveOptional(a_reader, savedCaptor);
                a_state.captureRelationships.push_back(record);
            }
            return a_length == 0;
        }

        bool ReadAISuspensions(OSFRecordReaderV1* a_reader, std::uint32_t a_length, LoadedState& a_state)
        {
            std::uint32_t count{};
            constexpr std::uint32_t kEntrySize = sizeof(FormID) + sizeof(std::uint8_t);
            if (!ReadValue(a_reader, a_length, count) || count > a_length / kEntrySize) {
                return false;
            }
            a_state.aiSuspensions.reserve(a_state.aiSuspensions.size() + count);
            for (std::uint32_t i = 0; i < count; ++i) {
                FormID savedActor{};
                std::uint8_t wasAIEnabled{};
                AISuspensionRecord record;
                if (!ReadValue(a_reader, a_length, savedActor) || !ReadValue(a_reader, a_length, wasAIEnabled) ||
                    wasAIEnabled > 1) {
                    return false;
                }
                if (ResolveRequired(a_reader, savedActor, record.actorId)) {
                    record.wasAIEnabled = wasAIEnabled != 0;
                    a_state.aiSuspensions.push_back(record);
                }
            }
            return a_length == 0;
        }

        void ApplyLoadedState(LoadedState a_state, std::uint64_t a_generation, std::uint64_t a_epoch)
        {
            if (g_loadGeneration.load(std::memory_order_acquire) != a_generation || !RuntimeMutation::IsCurrent(a_epoch)) {
                REX::WARN("[OSFDefeat] discarded stale deferred persistence load");
                return;
            }
            std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
            for (auto& record : a_state.defeated) {
                Defeat::RestoreFromSave(record.formId, record.registeredDay, record.aggressor,
                    record.autoRecoverAtDay, record.leaseExpiresAtDay, std::move(record.leaseHolderId), record.cause,
                    record.priorState);
            }
            for (const auto& record : a_state.ownedCaptiveFactions) {
                Engine::RestoreCaptiveFaction(record.actorId, record.factionId);
            }
            for (const auto& record : a_state.captureRelationships) {
                Capture::RestoreCapture(record.captiveId, record.captorId, record.capturedDay,
                    record.removeOwnedFactionOnRelease);
            }
            for (const auto& record : a_state.aiSuspensions) {
                Defeat::RestoreAISuspension(record.actorId, record.wasAIEnabled);
            }
            REX::INFO("[OSFDefeat] loaded state: {} active defeat, {} owned captive-faction membership, {} capture relationship, {} AI suspension record(s)",
                a_state.defeated.size(), a_state.ownedCaptiveFactions.size(), a_state.captureRelationships.size(),
                a_state.aiSuspensions.size());
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
                    if (type == kRecDefeated && (version == 1 || version == kDefeatedVersion)) {
                        ok = ReadDefeated(a_reader, version, length, state);
                    } else if (type == kRecOwnedCaptiveFaction && version == kOwnedCaptiveFactionVersion) {
                        ok = ReadOwnedCaptiveFactions(a_reader, length, state);
                    } else if (type == kRecCaptureRelationship &&
                               (version == 1 || version == kCaptureRelationshipVersion)) {
                        ok = ReadCaptureRelationships(a_reader, version, length, state);
                    } else if (type == kRecAISuspension && version == kAISuspensionVersion) {
                        ok = ReadAISuspensions(a_reader, length, state);
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
                const auto epoch = RuntimeMutation::CurrentEpoch();
                tasks->AddTask([state = std::move(state), generation, epoch]() mutable {
                    ApplyLoadedState(std::move(state), generation, epoch);
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
            const auto epoch = RuntimeMutation::AdvanceEpoch();
            const auto* const tasks = SFSE::GetTaskInterface();
            if (!tasks) {
                REX::ERROR("[OSFDefeat] task interface unavailable; runtime revert could not be applied safely");
                return;
            }
            tasks->AddTask([epoch]() {
                if (!RuntimeMutation::IsCurrent(epoch)) {
                    return;
                }
                std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
                // Events owns VM smart pointers and must be cleared here, never from the persistence
                // callback thread. The rest closes every session-scoped state seam before Load applies.
                Events::Clear();
                Defeat::ResetRuntimeState();
                Engine::ResetRuntimeState();
                Capture::Clear();
                Consequence::ResetRuntimeState();
                PlayerDefeat::ResetTransientState();
                DefeatContext::ClearPlayerContext();
                Presentation::ResetRuntimeState();
                Eligibility::SetEnabled(true);
                REX::INFO("[OSFDefeat] reverted all runtime defeat state (epoch {})", epoch);
            });
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
