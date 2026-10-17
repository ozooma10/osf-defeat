#include "pch.h"
#include "Engine/AttackEvents.h"
#include "Core/RuntimeMutation.h"
#include "Core/Truce.h"
#include "RE/E/Events.h"
#include "RE/M/MagicItem.h"
#include "REL/ASM.h"
#include "REL/Utility.h"
#ifdef OSF_DEFEAT_TESTING
#include "AcceptanceTests.h"
#endif

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

        using Cast = void(void*, RE::MagicItem*, bool, RE::TESObjectREFR*, float, bool, float, RE::Actor*);
        static_assert(offsetof(RE::MagicItem, hostileCount) == 0x138);

        void RemoteCast(void* caster, RE::MagicItem* spell, bool dual, RE::TESObjectREFR* target,
            float effectiveness, bool adjustArea, float magnitude, RE::Actor* blame)
        {
            const auto epoch = RuntimeMutation::CurrentEpoch();
            // Spell.IsHostile uses this signed count on .244. The focused suite
            // compares the member address/value with Papyrus for damage and healing.
            const bool hostile = spell && spell->hostileCount > 0;
            const FormID blameId = blame ? blame->GetFormID() : 0;
            const auto original = reinterpret_cast<Cast*>((*reinterpret_cast<std::uintptr_t**>(caster))[2]);
            original(caster, spell, dual, target, effectiveness, adjustArea, magnitude, blame);
#ifdef OSF_DEFEAT_TESTING
            RuntimeTests::Acceptance::ObservedRemoteCast(spell ? spell->GetFormID() : 0, blameId, hostile,
                *reinterpret_cast<const std::uintptr_t*>(caster) - reinterpret_cast<std::uintptr_t>(REX::W32::GetModuleHandleW(nullptr)));
#endif
            if (hostile) Truce::QueueEndForAttack(blameId, epoch);
        }

        bool InstallRemoteCast()
        {
            static bool installed = false;
            if (installed) return true;
            const auto image = reinterpret_cast<std::uintptr_t>(REX::W32::GetModuleHandleW(nullptr));
            const auto function = REL::ID{117902}.address();
            const auto site = function + 0x1EA;
            // Papyrus RemoteCast's deferred functor: source/caster/cell validation
            // has succeeded. All eight arguments are prepared, including blame at
            // [rsp+38]. Replace only `mov rcx,rdi; call [rax+10]` and forward the
            // same virtual method. No engine damage or attribution is rewritten.
            constexpr unsigned char callGate[]{0x4C,0x8B,0xCE,0x45,0x33,0xC0,0x48,0x8B,0x55,0xD8,0x48,0x8B,0xCF,0xFF,0x50,0x10,0xEB,0x51};
            constexpr unsigned char hostileGate[]{0x41,0x83,0xB8,0x38,0x01,0x00,0x00,0x00,0x0F,0x9F,0xC0,0xC3};
            if (function != image + 0x2044D40 ||
                std::memcmp(reinterpret_cast<const void*>(site - 10), callGate, sizeof(callGate)) != 0 ||
                std::memcmp(reinterpret_cast<const void*>(image + 0x1FE41E0), hostileGate, sizeof(hostileGate)) != 0) {
                REX::ERROR("[OSFDefeat] RemoteCast verification failed; explicit-blame truce fallback unavailable");
                return false;
            }
            auto& trampoline = REL::GetTrampoline();
            auto* gateway = static_cast<std::uint8_t*>(trampoline.allocate(3 + sizeof(REL::ASM::JMP14)));
            // Tail jump preserves the call site's stack arguments and alignment.
            std::memcpy(gateway, callGate + 10, 3);
            new (gateway + 3) REL::ASM::JMP14{reinterpret_cast<std::uintptr_t>(&RemoteCast)};
            const REL::ASM::CALL6 call{site, trampoline.allocate_branch6(reinterpret_cast<std::uintptr_t>(gateway))};
            if (!REL::WriteSafeData(site, call)) {
                REX::ERROR("[OSFDefeat] failed to patch RemoteCast truce observer");
                return false;
            }
            installed = true;
            REX::INFO("[OSFDefeat] installed explicit-blame RemoteCast truce observer");
            return true;
        }
    }

    static bool InstallWeaponFire()
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

    bool Install()
    {
        const bool weapon = InstallWeaponFire();
        const bool remote = InstallRemoteCast();
        return weapon && remote;
    }
}
