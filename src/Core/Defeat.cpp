#include "pch.h"

#include "Core/Defeat.h"

#include "Engine/EngineFns.h"

namespace OSFDefeat::Defeat
{
	namespace
	{
		Observer g_onDefeat;
		Observer g_onRescue;

		[[nodiscard]] float NowDay()
		{
			const auto* const cal = RE::Calendar::GetSingleton();
			return cal ? cal->GetDaysPassedExact() : 0.0f;
		}
	}

	void SetOnDefeat(Observer a_observer) { g_onDefeat = std::move(a_observer); }
	void SetOnRescue(Observer a_observer) { g_onRescue = std::move(a_observer); }

	bool DefeatActor(RE::Actor* a_victim, RE::Actor* a_aggressor)
	{
		if (!a_victim) {
			return false;
		}
		if (a_victim->IsDead()) {
			REX::WARN("[OSFDefeat] {:08X} is dead and cannot be defeated", a_victim->GetFormID());
			return false;
		}

		const FormID id = a_victim->GetFormID();
		const FormID aggressorId = a_aggressor ? a_aggressor->GetFormID() : 0;

		auto data = DefeatRegistry::Add(id, NowDay(), aggressorId);
		if (!data) {
			REX::WARN("[OSFDefeat] {:08X} is already defeated", id);
			return false;
		}

		if (a_victim->IsPlayerRef()) {
			// Player path = the validated PoC: essential -> bleedout, global combat gate.
			Engine::EnsureEssential(RE::PlayerCharacter::GetSingleton());
			Engine::SetCombatAIDisabled(true);
		} else {
			// NPC path — the OSF RE-proven, base-form-agnostic per-actor recipe
			// (gameplay.defeat_damage_hook, live-validated 2026-06-25). No shared-base flip,
			// no health hacking: force the bleedout life state, hold it with
			// kNoBleedoutRecovery, make it invulnerable so the defeat sticks through ongoing
			// fire, and remove it from combat at the manager level so other actors disengage.
			a_victim->boolFlags.set(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
			Engine::SetLifeState(a_victim, Engine::kBleedOut);
			Engine::SetGhost(a_victim, true);
			Engine::StopCombatAndAlarmOnActor(a_victim);
			REX::INFO("[OSFDefeat] NPC defeat (bleedout + ghost + disengage): {:08X}", id);
		}

		data->state.store(DefeatState::Defeated);
		REX::INFO("[OSFDefeat] defeated {:08X} (aggressor {:08X})", id, aggressorId);

		if (g_onDefeat) {
			g_onDefeat(id, aggressorId);
		}
		return true;
	}

	bool RescueActor(RE::Actor* a_victim, bool /*a_undoPacify*/)
	{
		if (!a_victim) {
			return false;
		}
		const FormID id = a_victim->GetFormID();

		auto data = DefeatRegistry::Get(id);
		if (!data) {
			REX::WARN("[OSFDefeat] {:08X} is not defeated and cannot be rescued", id);
			return false;
		}
		auto expected = DefeatState::Defeated;
		if (!data->state.compare_exchange_strong(expected, DefeatState::Rescuing)) {
			REX::INFO("[OSFDefeat] {:08X} rescue skipped (state busy)", id);
			return false;
		}

		if (a_victim->IsPlayerRef()) {
			Engine::RestoreHealthToMax(a_victim);
			Engine::SetCombatAIDisabled(false);
		} else {
			Engine::SetLifeState(a_victim, Engine::kAlive);
			Engine::RestoreHealthToMax(a_victim);  // exit bleedout + get up
			Engine::SetGhost(a_victim, false);     // drop invulnerability
			a_victim->boolFlags.reset(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
		}

		const FormID aggressorId = data->aggressor;
		DefeatRegistry::Remove(id);
		REX::INFO("[OSFDefeat] rescued {:08X}", id);

		if (g_onRescue) {
			g_onRescue(id, aggressorId);
		}
		return true;
	}

	bool IsDefeated(const RE::Actor* a_victim)
	{
		return a_victim && DefeatRegistry::Contains(a_victim->GetFormID());
	}

	bool IsDamageImmune(const RE::Actor* a_victim)
	{
		return IsDefeated(a_victim);
	}

	std::vector<RE::Actor*> GetAllDefeated(bool /*a_loadedOnly*/)
	{
		// TODO: honor a_loadedOnly once a 3D-loaded check is wired in.
		std::vector<RE::Actor*> out;
		for (const auto id : DefeatRegistry::Snapshot()) {
			if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
				out.push_back(actor);
			}
		}
		return out;
	}

	void RestoreFromSave(FormID a_id, float a_registeredDay, FormID a_aggressor)
	{
		auto data = DefeatRegistry::Add(a_id, a_registeredDay, a_aggressor);
		if (!data) {
			return;
		}
		data->state.store(DefeatState::Defeated);

		auto* const victim = RE::TESForm::LookupByID<RE::Actor>(a_id);
		if (!victim) {
			return;  // not loaded yet; keyword/flag reapplied when it loads (TODO: Load3D hook)
		}
		if (victim->IsPlayerRef()) {
			Engine::EnsureEssential(RE::PlayerCharacter::GetSingleton());
			Engine::SetCombatAIDisabled(true);
		} else {
			victim->boolFlags.set(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
			Engine::SetLifeState(victim, Engine::kBleedOut);
			Engine::SetGhost(victim, true);
			Engine::StopCombatAndAlarmOnActor(victim);
		}
	}

	void Pacify(RE::Actor* a_victim)
	{
		if (!a_victim) {
			return;
		}
		// Per-actor pacify = remove the actor from combat at the MANAGER level so every other
		// combatant disengages (OSF RE-proven). This is the per-actor analog of the global tcai
		// gate, and the lever plain per-actor StopCombat lacked (others re-acquire after StopCombat).
		Engine::StopCombatAndAlarmOnActor(a_victim);
		REX::INFO("[OSFDefeat] pacified {:08X}", a_victim->GetFormID());
	}

	void UndoPacify(RE::Actor* /*a_victim*/)
	{
		// Nothing to undo explicitly: once the actor is alive with AI active it re-evaluates
		// combat on its own (RescueActor restores life state + AI). No-op by design.
	}

	bool IsPacified(const RE::Actor* a_victim)
	{
		// A defeated actor is combat-removed for the duration of its defeat. (Standalone Pacify
		// of a non-defeated actor is not separately tracked.)
		return IsDefeated(a_victim);
	}
}
