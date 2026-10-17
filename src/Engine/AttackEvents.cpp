#include "pch.h"
#include "Engine/AttackEvents.h"
#include "Core/RuntimeMutation.h"
#include "Core/Truce.h"
#include "RE/E/Events.h"

namespace OSFDefeat::AttackEvents
{
    namespace
    {
        class WeaponFireSink final : public RE::BSTEventSink<RE::WeaponFiredEvent>
        {
            RE::BSEventNotifyControl ProcessEvent(const RE::WeaponFiredEvent& a_event,
                RE::BSTEventSource<RE::WeaponFiredEvent>*) override
            {
                if (const auto* actor = a_event.actor.get()) {
                    Truce::QueueEndForAttack(actor->GetFormID(), RuntimeMutation::CurrentEpoch());
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        } g_weaponFireSink;
    }

    bool Install()
    {
        static bool installed = false;
        if (installed) return true;
        auto* source = RE::WeaponFiredEvent::GetEventSource();
        const auto image = reinterpret_cast<std::uintptr_t>(REX::W32::GetModuleHandleW(nullptr));
        // .244 dispatcher ID61804 references this source. Producer ID61817
        // retains the actor at event+0x10. Proven with player and NPC Fire calls;
        // canonical evidence: OSF RE gameplay.defeat_damage_hook.
        constexpr unsigned char sourceInstruction[]{0x48,0x8D,0x0D,0xE9,0x6D,0xE6,0x04};
        if (reinterpret_cast<std::uintptr_t>(source) != image + 0x590A240 ||
            *reinterpret_cast<const std::uintptr_t*>(source) != image + 0x4B88FF0 ||
            std::memcmp(reinterpret_cast<const void*>(image + 0xAA3450), sourceInstruction, sizeof(sourceInstruction)) != 0) {
            REX::ERROR("[OSFDefeat] WeaponFired event verification failed; scripted-fire truce fallback unavailable");
            return false;
        }
        source->RegisterSink(&g_weaponFireSink);
        installed = true;
        REX::INFO("[OSFDefeat] installed native weapon-fire truce observer");
        return true;
    }
}
