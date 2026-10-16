#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::PlayerDefeat
{
    // Called once at kPostDataLoad, after Decision::Initialize.
    void Install();
    void BeginPlayerDefeat(RE::Actor* a_aggressor = nullptr, DefeatCause a_cause = DefeatCause::kScripted);
    void ResetTransientState();

    // Struggle primitive: a per-actor no-defeat grace window. While active, would-be-lethal hits
    // on the actor pass through the clamp untouched — vanilla lethality returns; they can win the
    // fight or genuinely die. Content builds the QTE/struggle beat around the defeat-imminent
    // event and grants this on a successful resist. a_seconds <= 0 clears the window. Refused for
    // an already-defeated actor (a resist window is not recovery). Thread-safe (POD map).
    bool SetResistWindow(RE::Actor* a_actor, float a_seconds);
    [[nodiscard]] bool IsResistWindowActive(RE::Actor* a_actor);
}
