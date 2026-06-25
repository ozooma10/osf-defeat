#include "pch.h"

#include "Engine/EngineFns.h"

namespace OSFDefeat::Engine
{
	namespace
	{
		// The engine's ActorValueOwner subobject sits at Actor+0x70 on 1.16.244.
		constexpr std::ptrdiff_t kActorValueOwnerOffset = 0x70;

		// `tcai` (ToggleCombatAI) console handler = AddrLib ID 66826. It writes the
		// global "all combat AI disabled" gate (DAT_1458d570d) via the instruction
		// `40 88 3D <disp32>` (mov byte [rip+disp], dil). The gate byte itself is NOT
		// AddrLib-indexed, so we resolve it by parsing that write off the handler -
		// which relocates with the handler's ID across game builds.
		constexpr std::uint64_t kTcaiHandlerID = 66826;

		std::uintptr_t g_combatAIGate = 0;  // cached, resolved lazily

		[[nodiscard]] bool IsReadable(std::uintptr_t a_addr, std::size_t a_size)
		{
			if (a_addr == 0 || a_size == 0) {
				return false;
			}
			MEMORY_BASIC_INFORMATION mbi{};
			if (::VirtualQuery(reinterpret_cast<LPCVOID>(a_addr), &mbi, sizeof(mbi)) == 0) {
				return false;
			}
			if (mbi.State != MEM_COMMIT) {
				return false;
			}
			const auto protect = mbi.Protect & 0xFF;
			return (mbi.Protect & PAGE_GUARD) == 0 && protect != PAGE_NOACCESS;
		}

		// Resolve (and cache) the global combat-AI gate by parsing the tcai handler's
		// flag-write. Returns 0 if the handler / instruction can't be found.
		[[nodiscard]] std::uintptr_t CombatAIGate()
		{
			if (g_combatAIGate != 0) {
				return g_combatAIGate;
			}
			const auto handler = REL::ID(kTcaiHandlerID).address();
			if (!IsReadable(handler, 0x60)) {
				return 0;
			}
			const auto* const p = reinterpret_cast<const std::uint8_t*>(handler);
			for (std::size_t i = 0; i + 7 <= 0x60; ++i) {
				if (p[i] == 0x40 && p[i + 1] == 0x88 && p[i + 2] == 0x3D) {  // mov [rip+disp],dil
					const auto disp = *reinterpret_cast<const std::int32_t*>(p + i + 3);
					const auto gate =
						handler + i + 7 + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(disp));
					if (IsReadable(gate, 1)) {
						g_combatAIGate = gate;
						return gate;
					}
				}
			}
			return 0;
		}
	}

	void SetCombatAIDisabled(bool a_disabled)
	{
		const auto gate = CombatAIGate();
		if (gate == 0) {
			REX::WARN("[OSFDefeat] combat-AI gate unresolved (tcai ID {}) - cannot {} combat",
				kTcaiHandlerID, a_disabled ? "suppress" : "restore");
			return;
		}
		*reinterpret_cast<volatile std::uint8_t*>(gate) = a_disabled ? 1u : 0u;
	}

	RE::ActorValueInfo* HealthAV()
	{
		static RE::ActorValueInfo* cached =
			RE::TESForm::LookupByEditorID<RE::ActorValueInfo>("Health");
		return cached;
	}

	RE::ActorValueOwner* AVOwnerOf(RE::Actor* a_actor)
	{
		return reinterpret_cast<RE::ActorValueOwner*>(
			reinterpret_cast<std::byte*>(a_actor) + kActorValueOwnerOffset);
	}

	float GetHealth(RE::Actor* a_actor)
	{
		auto* const av = HealthAV();
		if (!av || !a_actor) {
			return 0.0f;
		}
		return AVOwnerOf(a_actor)->GetActorValue(*av);
	}

	void RestoreHealthToMax(RE::Actor* a_actor)
	{
		auto* const av = HealthAV();
		if (!av || !a_actor) {
			return;
		}
		auto* const  avOwner = AVOwnerOf(a_actor);
		const float  maxHp = avOwner->GetPermanentActorValue(*av);
		const float  curHp = avOwner->GetActorValue(*av);
		if (maxHp > 0.0f) {
			avOwner->RestoreActorValue(*av, maxHp);  // clamps to max -> full heal
			REX::INFO("[OSFDefeat] heal {:.1f} -> ~{:.0f}", curHp, maxHp);
		}
	}

	void DriveHealthFloor(RE::Actor* a_actor)
	{
		auto* const av = HealthAV();
		if (!av || !a_actor) {
			return;
		}
		auto* const avOwner = AVOwnerOf(a_actor);
		const float hp = avOwner->GetActorValue(*av);
		if (hp > 0.0f) {
			avOwner->RestoreActorValue(*av, -hp + 0.05f);  // -> ~0 -> bleedout
		}
	}

	void EnsureEssential(RE::PlayerCharacter* a_player)
	{
		if (!a_player) {
			return;
		}
		auto* const npc = a_player->GetNPC();
		if (npc && !npc->IsEssential()) {
			npc->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kEssential);
			REX::INFO("[OSFDefeat] player base {:08X} set Essential", npc->GetFormID());
		}
	}
}
