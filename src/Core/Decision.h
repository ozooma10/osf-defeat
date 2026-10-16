#pragma once

#include "pch.h"

namespace OSFDefeat::Decision
{
    enum class ActorClass : std::int32_t
    {
        Flesh = 0,
        Robot = 1,
        Turret = 2
    };

    // Call once at kPostDataLoad, before installing damage hooks or Papyrus bindings.
    void Initialize();

    // Robot/turret discrimination via ActorType keywords (OSF RE defeat-3d-reattach Part B,
    // 2026-06-26): HasKeyword = ID 42794; ActorTypeRobot/ActorTypeTurret are ESM content resolved
    // by editorID. Classify checks turret first (more specific), then robot, else flesh.
    [[nodiscard]] ActorClass Classify(RE::Actor* a_actor);
    [[nodiscard]] bool       IsNonFlesh(RE::Actor* a_actor);

    // Hostility via Actor::IsHostileToActor (ID 100890).
    [[nodiscard]] bool IsHostile(RE::Actor* a_actor, RE::Actor* a_target);
}
