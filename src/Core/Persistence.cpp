#include "pch.h"

#include "Core/Persistence.h"

#include "Core/Defeat.h"
#include "Core/DefeatRegistry.h"
#include "Core/DefeatContext.h"
#include "Core/Events.h"
#include "Core/PlayerDefeat.h"
#include "Engine/Engine.h"

namespace OSFDefeat::Persistence
{
    namespace
    {
        constexpr std::uint32_t kUniqueID = 'OSFD';
        constexpr std::uint32_t kRecDefeated = 'DFT0';
        constexpr std::uint32_t kRecCaptive = 'CPT0';
        constexpr std::uint32_t kVersion = 2;
        constexpr std::uint32_t kCaptiveVersion = 1;

        void Save(const SFSE::SerializationInterface* a_intfc)
        {
            if (!a_intfc->OpenRecord(kRecDefeated, kVersion)) {
                REX::WARN("[OSFDefeat] failed to open save record");
                return;
            }

            const auto ids = DefeatRegistry::Snapshot();
            const auto count = static_cast<std::uint32_t>(ids.size());
            a_intfc->WriteRecordData(count);
            for (const auto id : ids) {
                const auto data = DefeatRegistry::Get(id);
                const FormID formId = id;
                const float day = data ? data->registeredDay : 0.0f;
                const FormID aggressor = data ? data->aggressor : 0;
                const float recoverAtDay = data ? data->recoverAtDay : 0.0f;
                a_intfc->WriteRecordData(formId);
                a_intfc->WriteRecordData(day);
                a_intfc->WriteRecordData(aggressor);
                a_intfc->WriteRecordData(recoverAtDay);
            }
            REX::INFO("[OSFDefeat] saved {} defeated actor(s)", count);

            // Captive-faction memberships WE created (actorID -> factionID), so a load mid-captivity
            // still removes exactly ours on recovery instead of leaking the membership.
            if (a_intfc->OpenRecord(kRecCaptive, kCaptiveVersion)) {
                const auto captives = Engine::SnapshotCaptiveFactions();
                const auto captiveCount = static_cast<std::uint32_t>(captives.size());
                a_intfc->WriteRecordData(captiveCount);
                for (const auto& [actorId, factionId] : captives) {
                    a_intfc->WriteRecordData(actorId);
                    a_intfc->WriteRecordData(factionId);
                }
                if (captiveCount > 0) {
                    REX::INFO("[OSFDefeat] saved {} captive-faction record(s)", captiveCount);
                }
            }
        }

        void Load(const SFSE::SerializationInterface* a_intfc)
        {
            std::uint32_t type{}, version{}, length{};
            while (a_intfc->GetNextRecordInfo(type, version, length)) {
                if (type == kRecCaptive) {
                    std::uint32_t captiveCount = 0;
                    a_intfc->ReadRecordData(captiveCount);
                    for (std::uint32_t i = 0; i < captiveCount; ++i) {
                        FormID actorId{}, factionId{};
                        a_intfc->ReadRecordData(actorId);
                        a_intfc->ReadRecordData(factionId);
                        const auto resolvedActor = a_intfc->ResolveFormID(actorId);
                        const auto resolvedFaction = a_intfc->ResolveFormID(factionId);
                        if (resolvedActor && resolvedFaction) {
                            Engine::RestoreCaptiveFaction(*resolvedActor, *resolvedFaction);
                        }
                    }
                    continue;
                }
                if (type != kRecDefeated) {
                    continue;
                }

                std::uint32_t count = 0;
                a_intfc->ReadRecordData(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    FormID formId{}, aggressor{};
                    float day{}, recoverAtDay{};
                    a_intfc->ReadRecordData(formId);
                    a_intfc->ReadRecordData(day);
                    a_intfc->ReadRecordData(aggressor);
                    if (version >= 2) {
                        a_intfc->ReadRecordData(recoverAtDay);
                    } else if (formId == 0x14) {
                        recoverAtDay = PlayerDefeat::RecoveryDeadlineForSeconds(8.0f);
                    }

                    const auto resolved = a_intfc->ResolveFormID(formId);
                    if (!resolved) {
                        REX::WARN("[OSFDefeat] could not resolve defeated formID {:08X}", formId);
                        continue;
                    }

                    FormID resolvedAggressor = 0;
                    if (aggressor != 0) {
                        if (const auto r = a_intfc->ResolveFormID(aggressor)) {
                            resolvedAggressor = *r;
                        }
                    }
                    Defeat::RestoreFromSave(*resolved, day, resolvedAggressor, recoverAtDay);
                }
                REX::INFO("[OSFDefeat] loaded {} defeated actor(s) from co-save", count);
            }
        }

        void Revert(const SFSE::SerializationInterface*)
        {
            DefeatRegistry::Clear();
            Events::Clear();
            Engine::ClearCaptiveFactions();
            REX::INFO("[OSFDefeat] reverted defeat state");
        }
    }

    void Register()
    {
        const auto* const intfc = SFSE::GetSerializationInterface();
        if (!intfc) {
            REX::WARN("[OSFDefeat] no SFSE SerializationInterface - persistence disabled");
            return;
        }
        intfc->SetUniqueID(kUniqueID);
        intfc->SetSaveCallback(Save);
        intfc->SetLoadCallback(Load);
        intfc->SetRevertCallback(Revert);
        REX::INFO("[OSFDefeat] persistence registered");
    }
}