#include "pch.h"

#include "Papyrus/Functions.h"

#include "RE/B/BSScriptUtil.h"     // BindNativeMethod template
#include "RE/G/GameVM.h"           // GameVM::GetSingleton()->GetVM()
#include "RE/I/IVirtualMachine.h"

#include "Core/Decision.h"
#include "Core/Defeat.h"
#include "Core/Pipeline.h"
#include "Core/Validation.h"
#include "Trigger/ITrigger.h"  // Trigger::DownEvent

namespace OSFDefeat::Papyrus
{
	namespace
	{
		// --- State verbs (marshaled onto the game thread; return = "accepted/queued") ---

		// OSFDefeat.DefeatActor(Actor akVictim, Actor akAggressor = None) global -> bool
		bool DefeatActor(std::monostate, RE::Actor* a_victim, RE::Actor* a_aggressor)
		{
			if (!a_victim) {
				return false;
			}
			const auto* const tasks = SFSE::GetTaskInterface();
			if (!tasks) {
				return false;
			}
			const std::uint32_t victimId    = a_victim->GetFormID();
			const std::uint32_t aggressorId = a_aggressor ? a_aggressor->GetFormID() : 0;
			tasks->AddTask([victimId, aggressorId]() {
				auto* const victim = RE::TESForm::LookupByID<RE::Actor>(victimId);
				if (!victim) {
					return;
				}
				auto* const aggressor = aggressorId ? RE::TESForm::LookupByID<RE::Actor>(aggressorId) : nullptr;
				Pipeline::OnDown(Trigger::DownEvent{ victim, aggressor });  // attribution preserved
			});
			return true;
		}

		// OSFDefeat.RescueActor(Actor akVictim, bool abUndoPacify = true) global -> bool
		bool RescueActor(std::monostate, RE::Actor* a_victim, bool a_undoPacify)
		{
			if (!a_victim) {
				return false;
			}
			const auto* const tasks = SFSE::GetTaskInterface();
			if (!tasks) {
				return false;
			}
			const std::uint32_t victimId = a_victim->GetFormID();
			tasks->AddTask([victimId, a_undoPacify]() {
				if (auto* const victim = RE::TESForm::LookupByID<RE::Actor>(victimId)) {
					Defeat::RescueActor(victim, a_undoPacify);
				}
			});
			return true;
		}

		// --- Queries (read-only; safe from a script thread) ---

		// OSFDefeat.IsDefeated(Actor akActor) global -> bool
		bool IsDefeated(std::monostate, RE::Actor* a_actor)
		{
			return Defeat::IsDefeated(a_actor);
		}

		// OSFDefeat.GetDefeated(bool abLoadedOnly = false) global -> Actor[]
		std::vector<RE::Actor*> GetDefeated(std::monostate, bool a_loadedOnly)
		{
			return Defeat::GetAllDefeated(a_loadedOnly);
		}

		// OSFDefeat.ClassifyActor(Actor akVictim, Actor akAggressor = None) global -> string
		// Classifies the DOWNED actor (Hostile/Crew/Security/Civilian/Unknown). Content branches
		// on the AGGRESSOR itself via base-game GetRace()/IsGuard() on the ref the event hands it.
		RE::BSFixedString ClassifyActor(std::monostate, RE::Actor* a_victim, RE::Actor* a_aggressor)
		{
			return Decision::ClassName(Decision::Classify(a_victim, a_aggressor));
		}

		// --- Guard seam: suspend processing around a content-authored scripted scene ---

		// OSFDefeat.DisableProcessing(bool abDisable) global
		void DisableProcessing(std::monostate, bool a_disable)
		{
			Validation::SetEnabled(!a_disable);
		}

		bool g_bound = false;
	}

	void Register()
	{
		if (g_bound) {
			return;
		}
		const auto vm = RE::GameVM::GetSingleton() ? RE::GameVM::GetSingleton()->GetVM() : nullptr;
		if (!vm) {
			REX::WARN("[OSFDefeat] Papyrus VM not ready - public API NOT bound (try a later SFSE message)");
			return;
		}
		vm->BindNativeMethod("OSFDefeat", "DefeatActor", &DefeatActor, std::nullopt, false);
		vm->BindNativeMethod("OSFDefeat", "RescueActor", &RescueActor, std::nullopt, false);
		vm->BindNativeMethod("OSFDefeat", "IsDefeated", &IsDefeated, std::nullopt, false);
		vm->BindNativeMethod("OSFDefeat", "GetDefeated", &GetDefeated, std::nullopt, false);
		vm->BindNativeMethod("OSFDefeat", "ClassifyActor", &ClassifyActor, std::nullopt, false);
		vm->BindNativeMethod("OSFDefeat", "DisableProcessing", &DisableProcessing, std::nullopt, false);
		g_bound = true;
		REX::INFO("[OSFDefeat] Papyrus API bound: DefeatActor, RescueActor, IsDefeated, GetDefeated, "
				  "ClassifyActor, DisableProcessing");
	}
}
