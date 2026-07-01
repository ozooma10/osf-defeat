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

    // Ownership lease. ClaimDefeat holds the down-state under a named owner with a watchdog
    // deadline (a_maxHoldSeconds <= 0 uses the config watchdogSeconds); re-claiming with the same
    // owner extends the deadline, a different owner is refused while the lease is live. If the
    // deadline lapses without an extension the recovery tick force-rescues and logs the defaulting
    // owner. ReleaseClaim (owner must match) ends the lease and resumes the default recovery
    // timer; RescueActor always works regardless of lease. Game thread.
    bool ClaimDefeat(RE::Actor* a_actor, std::string_view a_owner, float a_maxHoldSeconds);
    bool ReleaseClaim(RE::Actor* a_actor, std::string_view a_owner);
    [[nodiscard]] std::string GetClaimOwner(RE::Actor* a_actor);
}