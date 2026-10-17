#pragma once
#include "pch.h"
#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Truce
{
    struct Record
    {
        FormID actorId{};
        float expiresAtDay{};
        bool ownsFaction{};
    };

    // Game thread, under the runtime mutation lock. Recovery adopts the faction
    // it would otherwise remove; a pre-existing, unowned membership stays unowned.
    bool Begin(RE::Actor& a_actor, float a_seconds);
    bool End(RE::Actor& a_actor);
    [[nodiscard]] bool IsActive(FormID a_actorId);
    void Tick();
    void EndAll();
    [[nodiscard]] std::vector<Record> Snapshot();
    void Restore(Record a_record);
    void Reset();
}
