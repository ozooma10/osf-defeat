#pragma once

#include "pch.h"

namespace OSFDefeat::Recovery
{
    [[nodiscard]] float DeadlineForSeconds(float a_realSeconds);

    bool ScheduleAutomatic(RE::Actor* a_actor, float a_delaySeconds);
    bool Hold(RE::Actor* a_actor);
    [[nodiscard]] float SecondsRemaining(RE::Actor* a_actor);

    // Compatibility implementation for the original overloaded Papyrus SetRecoveryTimer verb.
    bool SetTimer(RE::Actor* a_actor, float a_seconds);

    // Every lease implies a recovery hold. Re-acquiring with the same holder ID extends its
    // watchdog deadline; a different live holder is refused. Explicit recovery overrides a lease.
    bool AcquireLease(RE::Actor* a_actor, std::string_view a_holderId, float a_timeoutSeconds);
    bool ReleaseLease(RE::Actor* a_actor, std::string_view a_holderId);
    [[nodiscard]] std::string GetLeaseHolder(RE::Actor* a_actor);

    // Game-thread recovery/deadline/watchdog pass.
    void Tick();
}
