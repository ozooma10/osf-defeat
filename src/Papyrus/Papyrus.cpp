#include "pch.h"

#include "Papyrus/Papyrus.h"

#include "Core/Decision.h"
#include "Core/Defeat.h"
#include "Engine/EngineFns.h"
#include "Trigger/HookTrigger.h"

#include <variant>

namespace OSFDefeat::Papyrus
{
	namespace
	{
		// State-mutating verbs touch game state and the VM may invoke a native off the main
		// thread, so marshal them onto the game thread (by FormID, re-resolved there).
		template <class Fn>
		void OnGameThread(Fn a_fn)
		{
			if (const auto* const tasks = SFSE::GetTaskInterface()) {
				tasks->AddTask(std::move(a_fn));
			}
		}

		// --- native functions (script class "OSFDefeat") -------------------------

		// Put an actor into the held defeat state (no auto-recovery — the caller owns the timing
		// and must RescueActor). Queued; returns true if accepted.
		bool DefeatActor(std::monostate, RE::Actor* a_actor)
		{
			if (!a_actor) {
				return false;
			}
			const auto id = a_actor->GetFormID();
			OnGameThread([id]() {
				if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
					Defeat::DefeatActor(actor, nullptr);
				}
			});
			return true;
		}

		bool RescueActor(std::monostate, RE::Actor* a_actor, bool a_undoPacify)
		{
			if (!a_actor) {
				return false;
			}
			const auto id = a_actor->GetFormID();
			OnGameThread([id, a_undoPacify]() {
				if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
					Defeat::RescueActor(actor, a_undoPacify);
				}
			});
			return true;
		}

		bool PacifyActor(std::monostate, RE::Actor* a_actor)
		{
			if (!a_actor) {
				return false;
			}
			const auto id = a_actor->GetFormID();
			OnGameThread([id]() {
				if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
					Defeat::Pacify(actor);
				}
			});
			return true;
		}

		bool IsDefeated(std::monostate, RE::Actor* a_actor)
		{
			return Defeat::IsDefeated(a_actor);  // read-only, thread-safe
		}

		// 0 Hostile, 1 Crew, 2 Security, 3 Civilian, 4 Unknown (mirrors Decision::EncounterClass).
		std::int32_t ClassifyActor(std::monostate, RE::Actor* a_actor)
		{
			return static_cast<std::int32_t>(Decision::Classify(a_actor, nullptr));
		}

		// Current Health as a fraction of max [0..1] — lets a consumer threshold a surrender without
		// needing the Health actor-value form (so its plugin stays masterless). Read-only.
		float GetHealthPercent(std::monostate, RE::Actor* a_actor)
		{
			auto* const av = Engine::HealthAV();
			if (!a_actor || !av) {
				return 1.0f;
			}
			auto* const  avo = Engine::AVOwnerOf(a_actor);
			const float  maxHp = avo->GetPermanentActorValue(*av);
			const float  curHp = avo->GetActorValue(*av);
			return maxHp > 0.0f ? (curHp / maxHp) : 1.0f;
		}

		void SetPlayerSurrenderThreshold(std::monostate, float a_fraction)
		{
			Trigger::SetPlayerSurrenderFraction(a_fraction);
		}

		float GetPlayerSurrenderThreshold(std::monostate)
		{
			return Trigger::GetPlayerSurrenderFraction();
		}
	}

	void Register()
	{
		const auto* const gameVM = RE::GameVM::GetSingleton();
		auto* const       vm = gameVM ? gameVM->GetVM() : nullptr;
		if (!vm) {
			REX::WARN("[OSFDefeat] no Papyrus VM - native API NOT registered");
			return;
		}

		// BindNativeMethod(object, function, func, taskletCallable, isLatent). None of ours are
		// latent (they return immediately; mutating ones queue a game-thread task).
		constexpr std::string_view kClass = "OSFDefeat";
		constexpr std::optional<bool> kTasklet{};  // engine default
		vm->BindNativeMethod(kClass, "DefeatActor", &DefeatActor, kTasklet, false);
		vm->BindNativeMethod(kClass, "RescueActor", &RescueActor, kTasklet, false);
		vm->BindNativeMethod(kClass, "PacifyActor", &PacifyActor, kTasklet, false);
		vm->BindNativeMethod(kClass, "IsDefeated", &IsDefeated, kTasklet, false);
		vm->BindNativeMethod(kClass, "ClassifyActor", &ClassifyActor, kTasklet, false);
		vm->BindNativeMethod(kClass, "GetHealthPercent", &GetHealthPercent, kTasklet, false);
		vm->BindNativeMethod(kClass, "SetPlayerSurrenderThreshold", &SetPlayerSurrenderThreshold, kTasklet, false);
		vm->BindNativeMethod(kClass, "GetPlayerSurrenderThreshold", &GetPlayerSurrenderThreshold, kTasklet, false);

		REX::INFO("[OSFDefeat] Papyrus API registered (class OSFDefeat)");
	}
}
