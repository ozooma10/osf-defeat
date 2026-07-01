#pragma once

#include "pch.h"

namespace OSFDefeat::PlayerDefeat
{
    void Install();
    void BeginPlayerDefeat(RE::Actor* a_aggressor = nullptr);
    void TickRecovery();

    [[nodiscard]] float RecoveryDeadlineForSeconds(float a_realSeconds);

    // Recovery handshake for content consumers (any defeated actor, player included).  a_seconds <= 0 holds the entry: no auto-recovery, content owns RescueActor.
    bool SetRecoveryTimer(RE::Actor* a_actor, float a_seconds);

    // Remaining real seconds until auto-recovery: > 0 counting down, -1 held (awaiting explicit rescue), 0 not defeated.
    [[nodiscard]] float GetRecoverySecondsRemaining(RE::Actor* a_actor);
}