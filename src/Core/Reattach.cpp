#include "pch.h"

#include "Core/Reattach.h"

#include "Core/Defeat.h"
#include "Core/DefeatRegistry.h"
#include "Engine/Engine.h"

#include "RE/N/NiAVObject.h"

#include "REL/ASM.h"
#include "REL/Utility.h"

namespace OSFDefeat::Reattach
{
    namespace
    {
        using Set3D_t = void(RE::Actor*, RE::NiAVObject*, bool, bool);

        // Actor::Set3D override impl (Actor primary-vtable slot 0xA8). a_new3D != null = ATTACH
        // (3D built+attached on (re)load); == null = DETACH. Entry-hook by raw ID, 5-byte steal
        // `48 89 5C 24 10` (mov [rsp+0x10],rbx) — a clean instruction boundary, exactly the JMP5
        // width so no NOP fill is needed. (OSF RE defeat-3d-reattach Part A, 2026-06-26)
        constexpr REL::ID kSet3DID{ 101138 };
        constexpr std::array<std::uint8_t, 5> kExpectedPrologue{ 0x48, 0x89, 0x5C, 0x24, 0x10 };
        constexpr std::size_t kStolenLength = kExpectedPrologue.size();

        Set3D_t* g_original = nullptr;
        std::atomic_bool g_hooked{ false };

        void QueueReassert(FormID a_id)
        {
            const auto* const tasks = SFSE::GetTaskInterface();
            if (!tasks) {
                return;
            }
            tasks->AddTask([a_id]() {
                auto* const actor = RE::TESForm::LookupByID<RE::Actor>(a_id);
                if (actor && Defeat::IsDefeated(actor)) {
                    // Idempotent: the essential snapshot is guarded by a one-shot flag, so this only
                    // re-applies bleedout + ghost + combat suppression on the freshly rebuilt actor.
                    Engine::SetPlayerDownState(actor);
                }
            });
        }

        void Set3DDetour(RE::Actor* a_actor, RE::NiAVObject* a_new3D, bool a_arg3, bool a_arg4)
        {
            // Let the engine finish the (re)attach first, then re-assert onto the fresh 3D next
            // frame. The thunk itself only reads the registry (shared lock) and queues — all actor
            // mutation is deferred to the game thread, matching the ModActorValue clamp discipline.
            if (g_original) {
                g_original(a_actor, a_new3D, a_arg3, a_arg4);
            }

            if (a_new3D && a_actor && Defeat::IsDefeated(a_actor)) {
                QueueReassert(a_actor->GetFormID());
            }
        }

        [[nodiscard]] bool InstallHook()
        {
            const auto target = kSet3DID.address();
            const auto* const bytes = reinterpret_cast<const std::uint8_t*>(target);
            if (!std::equal(kExpectedPrologue.begin(), kExpectedPrologue.end(), bytes)) {
                REX::ERROR("[OSFDefeat] Set3D prologue mismatch; 3D-reattach re-assert disabled");
                return false;
            }

            auto& trampoline = REL::GetTrampoline();
            auto* const gateway = static_cast<std::uint8_t*>(trampoline.allocate(kStolenLength + sizeof(REL::ASM::JMP14)));
            std::memcpy(gateway, bytes, kStolenLength);
            new (gateway + kStolenLength) REL::ASM::JMP14{ target + kStolenLength };
            g_original = reinterpret_cast<Set3D_t*>(gateway);

            const auto branch = trampoline.allocate_branch5(reinterpret_cast<std::uintptr_t>(&Set3DDetour));
            const REL::ASM::JMP5 jmp{ target, branch };
            if (!REL::WriteSafeData(target, jmp)) {
                REX::ERROR("[OSFDefeat] failed to patch Set3D hook");
                return false;
            }
            if constexpr (kStolenLength > sizeof(jmp)) {
                (void)REL::WriteSafeFill(target + sizeof(jmp), 0x90, kStolenLength - sizeof(jmp));
            }

            REX::INFO("[OSFDefeat] installed 3D-reattach re-assert hook at {:X}", target);
            return true;
        }
    }

    void Install()
    {
        if (g_hooked.exchange(true)) {
            return;
        }
        g_hooked.store(InstallHook());
    }
}
