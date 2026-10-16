#include "pch.h"

#include "Core/Reattach.h"

#include "Core/Defeat.h"
#include "Core/DefeatRegistry.h"
#include "Core/RuntimeMutation.h"
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

        void QueueReassert(FormID a_id)
        {
            auto* const tasks = RuntimeMutation::GetTaskQueue();
            if (!tasks) {
                return;
            }
            const auto epoch = RuntimeMutation::CurrentEpoch();
            tasks->AddTask([a_id, epoch]() {
                if (!RuntimeMutation::IsCurrent(epoch)) {
                    return;
                }
                std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
                auto* const actor = RE::TESForm::LookupByID<RE::Actor>(a_id);
                const auto defeat = DefeatRegistry::Get(a_id);
                if (actor && defeat) {
                    // Idempotent: ActiveDefeat already owns the prior-state snapshot, so this only
                    // re-applies bleedout + ghost + combat suppression on the rebuilt actor.
                    Engine::ApplyDownState(*actor, *defeat);
                }
            });
        }

        void Set3DDetour(RE::Actor* a_actor, RE::NiAVObject* a_new3D, bool a_arg3, bool a_arg4)
        {
            // Finish the (re)attach before submitting the re-assert. The native queue can run
            // inline on its drain owner, so submission must stay after the original call.
            g_original(a_actor, a_new3D, a_arg3, a_arg4);

            if (a_new3D && a_actor && Defeat::IsDefeated(a_actor)) {
                QueueReassert(a_actor->GetFormID());
            }
        }
    }

    void Install()
    {
        const auto target = kSet3DID.address();
        const auto* const bytes = reinterpret_cast<const std::uint8_t*>(target);
        if (!std::equal(kExpectedPrologue.begin(), kExpectedPrologue.end(), bytes)) {
            REX::ERROR("[OSFDefeat] Set3D prologue mismatch; 3D-reattach re-assert disabled");
            return;
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
            return;
        }
        if constexpr (kStolenLength > sizeof(jmp)) {
            (void)REL::WriteSafeFill(target + sizeof(jmp), 0x90, kStolenLength - sizeof(jmp));
        }

        REX::INFO("[OSFDefeat] installed 3D-reattach re-assert hook at {:X}", target);
    }
}
