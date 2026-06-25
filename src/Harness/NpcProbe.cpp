#include "pch.h"

#include "Harness/NpcProbe.h"

#include "Core/Defeat.h"
#include "Core/DefeatRegistry.h"
#include "Engine/EngineFns.h"

namespace OSFDefeat::Harness
{
	namespace
	{
		enum Phase : std::uint8_t
		{
			kAlive = 0,
			kDown = 1,
			kDead = 2
		};

		constexpr std::uint64_t kHeartbeatMs = 3000;

		std::atomic<bool>       g_enabled{ NpcProbe::kDefaultEnabled };
		std::set<FormID>        g_primed;
		std::map<FormID, Phase> g_phase;
		std::uint64_t           g_lastBeat = 0;

		[[nodiscard]] bool IsPrimable(RE::Actor* a_actor, RE::PlayerCharacter* a_player)
		{
			if (!a_actor || a_actor == a_player || a_actor->IsPlayerRef() || a_actor->IsDead()) {
				return false;
			}
			return !a_actor->boolFlags.all(RE::Actor::BOOL_FLAGS::kIsCommandedActor);
		}

		// Resolve the actor under the player's crosshair. This is a single stable
		// pointer read (PlayerCharacter::commandTarget, 0xF90) - unlike walking
		// ProcessLists::highActorHandles, which races the simulation thread's
		// reallocation of that BSTArray and crashed in BSPointerHandle::get().
		[[nodiscard]] RE::Actor* CrosshairActor(RE::PlayerCharacter* a_player)
		{
			auto* const ref = a_player->commandTarget;
			return (ref && ref->IsActor()) ? ref->As<RE::Actor>() : nullptr;
		}

		void Tick()
		{
			if (!g_enabled.load()) {
				return;
			}
			auto* const player = RE::PlayerCharacter::GetSingleton();
			if (!player || !Engine::HealthAV()) {
				return;
			}

			// 1. Prime the actor under the crosshair with the instance Essential flag.
			RE::Actor* const target = CrosshairActor(player);
			if (target && IsPrimable(target, player)) {
				const FormID id = target->GetFormID();
				if (g_primed.insert(id).second) {
					Engine::SetInstanceEssential(target, true);
					g_phase[id] = kAlive;
					REX::INFO("[OSFDefeat][probe] primed {:08X} essential (crosshair)", id);
				}
			}

			// 2. Watch primed NPCs for alive -> bleedout -> dead transitions.
			//    LookupByID is a stable hash lookup (no process-list walk).
			for (auto it = g_phase.begin(); it != g_phase.end();) {
				const FormID id = it->first;
				auto* const  actor = RE::TESForm::LookupByID<RE::Actor>(id);
				if (!actor) {
					g_primed.erase(id);
					it = g_phase.erase(it);
					continue;
				}
				if (actor->IsDead()) {
					if (it->second < kDead) {
						if (Defeat::IsDefeated(actor)) {
							REX::WARN("[OSFDefeat][probe] (2) FAILED: downed {:08X} DIED under damage", id);
						} else {
							REX::WARN("[OSFDefeat][probe] (1) FAILED: {:08X} died before bleedout (instance-essential did not hold)", id);
						}
						it->second = kDead;
					}
				} else if (Engine::IsInBleedout(actor)) {
					if (it->second < kDown) {
						REX::INFO("[OSFDefeat][probe] (1) HOLDS: {:08X} entered BLEEDOUT via instance-essential (hp={:.1f})",
							id, Engine::GetHealth(actor));
						it->second = kDown;
						if (!Defeat::IsDefeated(actor)) {
							Defeat::DefeatActor(actor, nullptr);  // left down (no recovery) to stress-test (2)
						}
					}
				}
				++it;
			}

			// 3. Heartbeat: prove the probe is alive and show the crosshair target state.
			const auto now = ::GetTickCount64();
			if (now - g_lastBeat >= kHeartbeatMs) {
				g_lastBeat = now;
				REX::INFO("[OSFDefeat][probe] heartbeat: primed={} tracked={}", g_primed.size(), g_phase.size());
				if (target) {
					REX::INFO("[OSFDefeat][probe]  crosshair {:08X} inCombat={} hostile={} dead={} bleedout={} hp={:.1f}",
						target->GetFormID(), target->IsInCombat(), target->IsHostileToActor(player),
						target->IsDead(), Engine::IsInBleedout(target), Engine::GetHealth(target));
				} else {
					REX::INFO("[OSFDefeat][probe]  no actor under crosshair (commandTarget null / non-actor)");
				}
			}
		}
	}

	void NpcProbe::Install()
	{
		const auto* const tasks = SFSE::GetTaskInterface();
		if (!tasks) {
			REX::WARN("[OSFDefeat][probe] no SFSE TaskInterface - probe disabled");
			return;
		}
		tasks->AddPermanentTask([]() { Tick(); });
		REX::INFO("[OSFDefeat][probe] installed (enabled={})", g_enabled.load());
	}

	void NpcProbe::SetEnabled(bool a_enabled) { g_enabled.store(a_enabled); }
	bool NpcProbe::IsEnabled() { return g_enabled.load(); }
}
