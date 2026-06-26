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

		// Per-actor NPC defeat primitives — AddrLib IDs RE'd + live-validated in the OSF RE
		// sandbox (gameplay.defeat_damage_hook, 1.16.244).
		constexpr std::uint64_t kSetLifeStateID = 100787;  // void(Actor*, ACTOR_LIFE_STATE); vtable slot 0x16F
		constexpr std::uint64_t kSetGhostID = 100873;      // void(Actor*, bool)
		constexpr std::uint64_t kCombatRemoveID = 103285;  // void(ProcessLists*, Actor*, bool)

		// EM/stun depletes a dedicated "Electromagnetic Health" AV (NOT Health). It's resolved off
		// BGSDefaultObjectManager: GetSingleton (ID 43134) returns the manager; the EM Health
		// ActorValueInfo* lives at mgr + 0xd0 on 1.16.244. Getter ID is stable; re-anchor the slot
		// on a game update. (OSF RE em-stun-damage-routing.)
		constexpr std::uint64_t  kDefaultObjectManagerSingletonID = 43134;
		constexpr std::ptrdiff_t kEMHealthAVSlot = 0xd0;

		// The actor's current combat target = a raw u32 TESPointerHandle at Actor+0x298 (CLSF
		// models the field but exposes no accessor). RE-proven on 1.16.244 (StopCombat clears it;
		// gameplay.defeat_damage_hook). Sits right beside dialogueItemTarget@0x294 / myKiller@0x29C.
		constexpr std::ptrdiff_t kCurrentCombatTargetHandleOffset = 0x298;

		// Taxonomy reads (OSF RE actor_faction_taxonomy, live-validated 1.16.244): the actor's base
		// form pointer sits at Actor+0x98, and the base's crime faction (TESNPC jurisdiction) at
		// base+0x3B0. (CLSF's modeled offsets here were found stale — read raw.)
		constexpr std::ptrdiff_t kActorBaseOffset = 0x98;
		constexpr std::ptrdiff_t kCrimeFactionOffset = 0x3B0;

		// Resolve a raw u32 TESPointerHandle stored at a fixed Actor offset to an Actor*, via the
		// engine handle manager. GAME-THREAD ONLY (reads the handle table under its reader lock).
		[[nodiscard]] RE::Actor* ResolveActorHandleAt(RE::Actor* a_actor, std::ptrdiff_t a_offset)
		{
			if (!a_actor) {
				return nullptr;
			}
			// A plain field load off an Actor pointer we own (NOT a live process-list array walk),
			// so the read itself is safe; the handle-TABLE resolution below is the game-thread part.
			const std::uint32_t handle = *reinterpret_cast<const std::uint32_t*>(
				reinterpret_cast<const std::byte*>(a_actor) + a_offset);
			if (handle == 0) {  // empty handle (index 0 is the invalid slot)
				return nullptr;
			}
			// BSPointerHandle<Actor> is layout-identical to the raw u32, so reinterpret the handle
			// storage into the CLSF handle type and resolve through its .get() -> GetSmartPointer
			// (ID 35638). The resolver age-checks the table and yields null for a stale/garbage
			// handle (no crash). NiPointer balances the engine's AddRef on scope-exit; the referent
			// is a live game actor held by the engine, so the raw pointer stays valid this frame.
			static_assert(sizeof(RE::BSPointerHandle<RE::Actor>) == sizeof(std::uint32_t),
				"actor handle must be layout-identical to the raw u32");
			const auto&                    h = *reinterpret_cast<const RE::BSPointerHandle<RE::Actor>*>(&handle);
			const RE::NiPointer<RE::Actor> ptr = h.get();
			return ptr.get();
		}

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

	RE::ActorValueInfo* EMHealthAV()
	{
		// Resolve once and cache — but NEVER cache a null, so an early probe (e.g. the install-time
		// log) can't poison the cache before default objects are populated. The damage thunk calls
		// this from worker threads, so use an atomic; racers all compute the same pointer and the
		// redundant pre-cache getter calls are idempotent.
		static std::atomic<RE::ActorValueInfo*> cached{ nullptr };
		auto* av = cached.load(std::memory_order_relaxed);
		if (!av) {
			using GetSingletonFn = void* (*)();
			static REL::Relocation<GetSingletonFn> getSingleton{ REL::ID(kDefaultObjectManagerSingletonID) };
			if (auto* const mgr = static_cast<std::byte*>(getSingleton())) {
				av = *reinterpret_cast<RE::ActorValueInfo**>(mgr + kEMHealthAVSlot);
				if (av) {
					cached.store(av, std::memory_order_relaxed);
				}
			}
		}
		return av;
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

	void SetInstanceEssential(RE::Actor* a_actor, bool a_essential)
	{
		if (!a_actor) {
			return;
		}
		if (a_essential) {
			a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kEssential);
		} else {
			a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
		}
	}

	bool IsInBleedout(RE::Actor* a_actor)
	{
		return a_actor && a_actor->boolFlags.all(RE::Actor::BOOL_FLAGS::kInBleedoutAnimation);
	}

	void SetLifeState(RE::Actor* a_actor, std::uint32_t a_lifeState)
	{
		if (!a_actor) {
			return;
		}
		using Fn = void (*)(RE::Actor*, std::uint32_t);
		static REL::Relocation<Fn> fn{ REL::ID(kSetLifeStateID) };
		fn(a_actor, a_lifeState);
	}

	void SetGhost(RE::Actor* a_actor, bool a_ghost)
	{
		if (!a_actor) {
			return;
		}
		using Fn = void (*)(RE::Actor*, bool);
		static REL::Relocation<Fn> fn{ REL::ID(kSetGhostID) };
		fn(a_actor, a_ghost);
	}

	void StopCombatAndAlarmOnActor(RE::Actor* a_actor)
	{
		auto* const pl = RE::ProcessLists::GetSingleton();
		if (!a_actor || !pl) {
			return;
		}
		using Fn = void (*)(RE::ProcessLists*, RE::Actor*, bool);
		static REL::Relocation<Fn> fn{ REL::ID(kCombatRemoveID) };
		fn(pl, a_actor, false);
	}

	RE::Actor* ResolveCombatTarget(RE::Actor* a_actor)
	{
		return ResolveActorHandleAt(a_actor, kCurrentCombatTargetHandleOffset);
	}

	bool HasCrimeFaction(const RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		// Chase Actor+0x98 (base form) then base+0x3B0 (crimeFaction) as raw pointer-sized reads
		// off stable pointers — safe on any thread (no handle table, no array walk).
		const std::uintptr_t base = *reinterpret_cast<const std::uintptr_t*>(
			reinterpret_cast<const std::byte*>(a_actor) + kActorBaseOffset);
		if (base == 0) {
			return false;
		}
		const std::uintptr_t crimeFaction =
			*reinterpret_cast<const std::uintptr_t*>(base + kCrimeFactionOffset);
		return crimeFaction != 0;
	}

	bool HasCombatTarget(const RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		return *reinterpret_cast<const std::uint32_t*>(
				   reinterpret_cast<const std::byte*>(a_actor) + kCurrentCombatTargetHandleOffset) != 0u;
	}
}
