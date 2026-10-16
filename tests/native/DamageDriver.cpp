// Include the unmodified production translation unit so tests can drive its private
// detour directly. No fake installer or copied clamp algorithm is used.
#include "../../src/Core/PlayerDefeat.cpp"
#include "Test.h"
namespace Test {
    void Hit(RE::Actor& actor, float delta, RE::TESObjectREFR* aggressor,
        RE::ACTOR_VALUE_MODIFIER modifier, const RE::ActorValueInfo* gauge) {
        OSFDefeat::PlayerDefeat::g_original = [](RE::ActorValueOwner* owner, RE::ACTOR_VALUE_MODIFIER,
            const RE::ActorValueInfo&, float amount, RE::TESObjectREFR*) {
            owner->health += amount;
            if (onOriginalDamage) onOriginalDamage();
        };
        OSFDefeat::PlayerDefeat::ModActorValueDetour(&actor,modifier,gauge ? *gauge : *Engine::HealthAV(),delta,aggressor);
    }
}
