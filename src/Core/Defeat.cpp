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
			// NPC path (Phase-1 milestone). kNoBleedoutRecovery is available and the
			// health-floor drop works; what still needs RE is *per-actor* combat
			// suppression / pacify so we don't have to use the global gate. Best-effort.
			a_victim->boolFlags.set(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
			Engine::DriveHealthFloor(a_victim);
			REX::INFO("[OSFDefeat] NPC defeat is best-effort (per-actor pacify TODO): {:08X}", id);
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
			a_victim->boolFlags.reset(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
			Engine::RestoreHealthToMax(a_victim);
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
		}
	}

	void Pacify(RE::Actor* /*a_victim*/)
	{
		REX::INFO("[OSFDefeat] Pacify: per-actor pacify not yet implemented (TODO)");
	}

	void UndoPacify(RE::Actor* /*a_victim*/)
	{
		REX::INFO("[OSFDefeat] UndoPacify: per-actor pacify not yet implemented (TODO)");
	}

	bool IsPacified(const RE::Actor* /*a_victim*/)
	{
		return false;
	}
}
