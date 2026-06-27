#include "pch.h"

#include "Core/Decision.h"

#include "RE/B/BGSKeyword.h"

namespace OSFDefeat::Decision
{
    namespace
    {
        // ActorTypeRobot/ActorTypeTurret are pure ESM content (no engine default object, no exe
        // string), so resolve by editorID. Cache the first non-null hit — the lookup can return null
        // before forms finish loading, so retry until it resolves.
        RE::BGSKeyword* RobotKeyword()
        {
            static RE::BGSKeyword* cached = nullptr;
            if (!cached) {
                cached = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeRobot");
            }
            return cached;
        }

        RE::BGSKeyword* TurretKeyword()
        {
            static RE::BGSKeyword* cached = nullptr;
            if (!cached) {
                cached = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeTurret");
            }
            return cached;
        }
    }

    ActorClass Classify(RE::Actor* a_actor)
    {
        if (a_actor) {
            if (auto* const turret = TurretKeyword(); turret && a_actor->HasKeyword(turret)) {
                return ActorClass::Turret;
            }
            if (auto* const robot = RobotKeyword(); robot && a_actor->HasKeyword(robot)) {
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
