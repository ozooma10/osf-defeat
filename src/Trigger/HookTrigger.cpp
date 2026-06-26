#include "pch.h"

#include "Trigger/HookTrigger.h"

#include "Core/Defeat.h"
#include "Core/Validation.h"
#include "Engine/EngineFns.h"
#include "Engine/Hook.h"

#include <array>

namespace OSFDefeat::Trigger
{
	namespace
	{
		// =====================================================================
		// The RE'd seam (OSF RE `gameplay.defeat_damage_hook`, PHASE 4, live-validated
		// 2026-06-25 on 1.16.244):
		//
		//   ActorValueOwner::ModActorValue slot-6 impl = AddrLib ID 101587 = 0x1419774f0
		//     void(ActorValueOwner* /*rcx*/, ACTOR_VALUE_MODIFIER mod /*edx*/,
		//          const ActorValueInfo& info /*r8*/, float delta /*xmm3*/,
		//          TESObjectREFR* a_ref /*5th arg, stack*/)
		//
		// The slot-7 wrapper (ID 101588, a_ref = null) tail-calls slot-6, so an entry hook
		// here catches BOTH the wrapper (Papyrus/script) and the direct combat callers. This
		// is the universal incoming-AV-write point: damage is written here BEFORE the engine's
		// terminal decision reads the post value, so clamping the delta so post > 0 means the
		// terminal path never triggers. Live proof: NPC preHp=7.1, delta=-11.5 -> clamped ->
		// survived -> held bleedout, SetLifeState deathSeen=0, aggressor attributed.
		//
		// TWO AVs reach a terminal state through this same primitive (OSF RE em-stun-damage-routing,
		// 2026-06-25): Health (-> death) and Electromagnetic Health (-> the vanilla EM
		// shock/incapacitation). EM/stun depletes a SEPARATE "Electromagnetic Health" gauge, not
		// Health, so the filter recognizes both AVIFs and clamps them identically — the engine's
		// terminal state (death OR shock) never fires and the victim routes into our one unified
		// DefeatActor state. (CLAMP, not ADOPT: keeps a single framework-owned defeat/recovery
		// model rather than layering our state on top of the engine's behavior-graph shock.)
		//
		// DEAD ENDS (proven false in the sandbox — do not retry): hooking the keystone
		// HandleHealthDamage (ID 63465 — post-write, HP already <= 0) and rewriting
		// SetLifeState kDying->kBleedOut (death commits through more than the lifeState; the
		// essential->bleedout reroute is base-flag-gated so instance flips don't take).
		// Return/negate convention: NOT a negate — always forward to the gateway; the only
		// lever is the clamped `delta`. The actor is left alive at floor HP (no death race).
		// =====================================================================
		constexpr std::uint64_t kModActorValueID = 101587;

		// Entry-detour steals 5 bytes (one instruction, clean boundary): mov [rsp+8],rbx.
		// Verify-gate is 9 bytes for drift safety: mov [rsp+8],rbx ; mov [rsp+0x10],edx.
		constexpr std::size_t                  kStealBytes = 5;
		constexpr std::array<std::uint8_t, 9>  kModActorValueGate{
			0x48, 0x89, 0x5C, 0x24, 0x08, 0x89, 0x54, 0x24, 0x10
		};

		// Where a clamped would-be-terminal hit lands the victim's gauge (Health or EM Health):
		// clearly above 0, so neither the death path nor the EM shock fires.
		constexpr float kDownFloor = 5.0f;

		// The engine ABI of the hooked function (see the seam note above). Used for both the
		// detour signature and the gateway call — identical, so a clamped re-entry is a normal
		// ModActorValue with a smaller delta.
		using ModActorValueFn = void (*)(
			RE::ActorValueOwner*, RE::ACTOR_VALUE_MODIFIER, const RE::ActorValueInfo&, float, RE::TESObjectREFR*);

		ModActorValueFn g_gateway = nullptr;  // call to run the original ModActorValue
		ITrigger::Sink  g_sink;               // copied from the instance on Install()

		// Worker-thread -> game-thread handoff. The clamp runs on the damage worker thread
		// (arithmetic + forwarding the call = safe there). The held-bleedout treatment
		// (DefeatActor: SetLifeState/SetGhost/combat-remove) MUST run on the game thread, so we
		// only record {victim, aggressor} here and apply it from the per-frame task.
		struct PendingDown
		{
			FormID victim;
			FormID aggressor;
		};
		std::mutex                g_pendingMutex;
		std::vector<PendingDown>  g_pending;
		std::set<FormID>          g_pendingIds;  // dedupe re-clamps in the alive-at-floor window

		// The ActorValueOwner subobject is embedded at Actor+0x70 (EngineFns::AVOwnerOf); the
		// hook hands us the owner, so recover the Actor by reversing that offset.
		[[nodiscard]] RE::Actor* ActorFromAVOwner(RE::ActorValueOwner* a_avo)
		{
			return reinterpret_cast<RE::Actor*>(reinterpret_cast<std::byte*>(a_avo) - 0x70);
		}

		[[nodiscard]] RE::Actor* ResolveAggressor(RE::TESObjectREFR* a_ref)
		{
			// Direct combat path (projectile / melee / explosion): a_ref IS the damage dealer
			// (the seam body validates it via cmp [a_ref+0x2e],0x4b). Live-proven attribution.
			if (a_ref && a_ref->IsActor()) {
				return a_ref->As<RE::Actor>();
			}
			// Script / Papyrus DamageValue (the slot-7 wrapper) passes a_ref = null. We do NOT
			// resolve the victim's myKiller handle here — that handle-table deref is game-thread
			// only. Return null now; Drain (game thread) falls back to Engine::ResolveKiller on
			// the victim before firing the sink.
			return nullptr;
		}

		// THE DETOUR. Reached via JMP at the function entry, so registers/stack are exactly
		// what the engine set up for ModActorValue. Runs on the damage worker thread.
		void ModActorValueThunk(
			RE::ActorValueOwner* a_avo, RE::ACTOR_VALUE_MODIFIER a_mod, const RE::ActorValueInfo& a_info,
			float a_delta, RE::TESObjectREFR* a_ref)
		{
			float finalDelta = a_delta;

			// Fast filter: only a would-be-terminal write while the framework is enabled. delta<0
			// is the cheap first gate; then match the AV against the two gauges that reach a
			// terminal state through this primitive — Health (death) and EM Health (EM shock).
			if (a_delta < 0.0f && Validation::CanProcess()) {
				const bool isHealth = (Engine::HealthAV() == &a_info);
				const bool isEM = !isHealth && (Engine::EMHealthAV() == &a_info);
				RE::Actor* const victim = ActorFromAVOwner(a_avo);
				// NPC scope: the player keeps its proven essential->bleedout + PollTrigger path.
				if ((isHealth || isEM) && victim && !victim->IsPlayerRef() && !Defeat::IsDefeated(victim)) {
					const float preAV = a_avo->GetActorValue(a_info);
					if (preAV > 0.0f && preAV + a_delta <= 0.0f) {
						RE::Actor* const aggressor = ResolveAggressor(a_ref);
						// Candidacy == the pipeline's gate, so a clamp always yields a defeat
						// (no actor left stranded alive-but-undefeated at the floor).
						if (Validation::ValidatePair(victim, aggressor)) {
							// CLAMP: post-hit gauge = floor (> 0) -> the terminal path never fires.
							finalDelta = kDownFloor - preAV;

							const FormID vId = victim->GetFormID();
							const FormID aId = aggressor ? aggressor->GetFormID() : 0;
							{
								std::scoped_lock lk{ g_pendingMutex };
								if (g_pendingIds.insert(vId).second) {
									g_pending.push_back({ vId, aId });
								}
							}
							REX::INFO(
								"[OSFDefeat] hook: would-be-down {} hit on {:08X} (pre={:.1f} dmg={:.1f}) clamped "
								"to floor {:.1f}; aggressor {:08X}; prevented {} -> defeat",
								isEM ? "EM" : "Health", vId, preAV, a_delta, kDownFloor, aId,
								isEM ? "EM shock" : "death");
						}
					}
				}
			}

			g_gateway(a_avo, a_mod, a_info, finalDelta, a_ref);
		}

		// Game-thread drain (per-frame SFSE task): route each queued down through the existing
		// pipeline. The victim is alive at the floor gauge, so there is no death race with this defer.
		void Drain()
		{
			std::vector<PendingDown> batch;
			{
				std::scoped_lock lk{ g_pendingMutex };
				if (g_pending.empty()) {
					return;
				}
				batch.swap(g_pending);
			}
			for (const auto& down : batch) {
				auto* const victim = RE::TESForm::LookupByID<RE::Actor>(down.victim);
				auto*       aggressor =
					down.aggressor ? RE::TESForm::LookupByID<RE::Actor>(down.aggressor) : nullptr;
				// Script/Papyrus path: a_ref was null on the worker thread, so no aggressor was
				// captured. Now on the game thread we can safely resolve the victim's combat target
				// (Actor+0x298, "who they were fighting") through the handle table. We use the
				// combat target, NOT myKiller — the clamp prevents the kill, so myKiller is never
				// set for a clamped victim (confirmed in-game). Combat kills are already attributed
				// via a_ref, so this only ever upgrades the unattributed (script) path.
				if (victim && !aggressor) {
					if (auto* const target = Engine::ResolveCombatTarget(victim)) {
						aggressor = target;
						REX::INFO(
							"[OSFDefeat] hook: script-path aggressor resolved via combatTarget -> {:08X} for victim {:08X}",
							target->GetFormID(), down.victim);
					}
				}
				if (victim && g_sink) {
					g_sink(DownEvent{ victim, aggressor });  // -> Pipeline::OnDown -> DefeatActor
				}
				std::scoped_lock lk{ g_pendingMutex };
				g_pendingIds.erase(down.victim);  // re-arm: future hits re-evaluate (now IsDefeated)
			}
		}
	}

	void HookTrigger::Install()
	{
		g_sink = _sink;

		const auto gateway = Engine::Hook::InstallEntry<kStealBytes>(
			kModActorValueID, "ModActorValue(slot6) entry", kModActorValueGate, &ModActorValueThunk);
		if (gateway == 0) {
			REX::WARN(
				"[OSFDefeat] HookTrigger NOT installed (ModActorValue ID {} gate failed) - NPCs have no "
				"trigger; player still covered by PollTrigger",
				kModActorValueID);
			return;
		}
		g_gateway = reinterpret_cast<ModActorValueFn>(gateway);

		const auto* const tasks = SFSE::GetTaskInterface();
		if (!tasks) {
			REX::WARN("[OSFDefeat] HookTrigger: no SFSE TaskInterface - downs cannot be drained");
			return;
		}
		tasks->AddPermanentTask([]() { Drain(); });

		REX::INFO(
			"[OSFDefeat] HookTrigger installed (ModActorValue clamp: Health->death + EM Health->shock; "
			"NPC scope; EM AVIF resolved={})",
			Engine::EMHealthAV() != nullptr);
	}
}
