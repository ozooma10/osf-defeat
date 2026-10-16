#include "pch.h"

#include "Core/Decision.h"

#include "RE/B/BGSKeyword.h"

namespace OSFDefeat::Decision
{
    namespace
    {
        // Set once after data load, before damage hooks and Papyrus bindings can classify actors.
        RE::BGSKeyword* g_robotKeyword = nullptr;
        RE::BGSKeyword* g_turretKeyword = nullptr;
    }

    void Initialize()
    {
        g_robotKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeRobot");
        g_turretKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeTurret");
        if (!g_robotKeyword) {
            REX::WARN("[OSFDefeat] ActorTypeRobot keyword unavailable; robot classification disabled");
        }
        if (!g_turretKeyword) {
            REX::WARN("[OSFDefeat] ActorTypeTurret keyword unavailable; turret classification disabled");
        }
    }

    ActorClass Classify(RE::Actor* a_actor)
    {
        if (a_actor) {
            if (g_turretKeyword && a_actor->HasKeyword(g_turretKeyword)) {
                return ActorClass::Turret;
            }
            if (g_robotKeyword && a_actor->HasKeyword(g_robotKeyword)) {
                return ActorClass::Robot;
            }
        }
        return ActorClass::Flesh;
    }

    bool IsNonFlesh(RE::Actor* a_actor)
    {
        return Classify(a_actor) != ActorClass::Flesh;
    }

    bool IsHostile(RE::Actor* a_actor, RE::Actor* a_target)
    {
        return a_actor && a_target && a_actor->IsHostileToActor(a_target);
    }
}
