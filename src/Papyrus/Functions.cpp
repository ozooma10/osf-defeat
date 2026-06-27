#include "pch.h"

#include "Papyrus/Functions.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IVirtualMachine.h"

#include "Core/Defeat.h"
#include "Core/Validation.h"

#include <variant>

namespace OSFDefeat::Papyrus
{
	namespace
	{
		template <class Fn>
		bool QueueGameTask(Fn a_fn)
		{
			const auto* const tasks = SFSE::GetTaskInterface();
			if (!tasks) {
				return false;
			}
			tasks->AddTask(std::move(a_fn));
			return true;
		}

		bool DefeatActor(std::monostate, RE::Actor* a_victim, RE::Actor* a_aggressor)
		{
			if (!a_victim) {
				return false;
			}

			const FormID victimId = a_victim->GetFormID();
			const FormID aggressorId = a_aggressor ? a_aggressor->GetFormID() : 0;
			return QueueGameTask([victimId, aggressorId]() {
				auto* const victim = RE::TESForm::LookupByID<RE::Actor>(victimId);
				auto* const aggressor = aggressorId ? RE::TESForm::LookupByID<RE::Actor>(aggressorId) : nullptr;
				if (Validation::CanProcess() && Validation::ValidatePair(victim, aggressor)) {
					Defeat::DefeatActor(victim, aggressor);
				}
			});
		}

		bool RescueActor(std::monostate, RE::Actor* a_victim, bool a_undoPacify)
		{
			if (!a_victim) {
				return false;
			}

			const FormID victimId = a_victim->GetFormID();
			return QueueGameTask([victimId, a_undoPacify]() {
				if (auto* const victim = RE::TESForm::LookupByID<RE::Actor>(victimId)) {
					Defeat::RescueActor(victim, a_undoPacify);
				}
			});
		}

		bool PacifyActor(std::monostate, RE::Actor* a_actor)
		{
			if (!a_actor) {
				return false;
			}

			const FormID actorId = a_actor->GetFormID();
			return QueueGameTask([actorId]() {
				if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
					Defeat::Pacify(actor);
				}
			});
		}

		bool ReleaseActor(std::monostate, RE::Actor* a_actor)
		{
			if (!a_actor) {
				return false;
			}

			const FormID actorId = a_actor->GetFormID();
			return QueueGameTask([actorId]() {
				if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
					Defeat::UndoPacify(actor);
				}
			});
		}

		bool IsDefeated(std::monostate, RE::Actor* a_actor)
		{
			return Defeat::IsDefeated(a_actor);
		}

		bool IsPacified(std::monostate, RE::Actor* a_actor)
		{
			return Defeat::IsPacified(a_actor);
		}

		std::vector<RE::Actor*> GetDefeated(std::monostate, bool a_loadedOnly)
		{
			return Defeat::GetAllDefeated(a_loadedOnly);
		}

		void DisableProcessing(std::monostate, bool a_disable)
		{
			Validation::SetEnabled(!a_disable);
		}

		bool IsProcessingDisabled(std::monostate)
		{
			return !Validation::IsEnabled();
		}

		bool g_bound = false;
	}

	void Register()
	{
		if (g_bound) {
			return;
		}

		const auto* const gameVM = RE::GameVM::GetSingleton();
		auto* const vm = gameVM ? gameVM->GetVM() : nullptr;
		if (!vm) {
			REX::WARN("[OSFDefeat] Papyrus VM not ready - API not bound");
			return;
		}

		constexpr std::string_view kClass = "OSFDefeat";
		vm->BindNativeMethod(kClass, "DefeatActor", &DefeatActor, std::nullopt, false);
		vm->BindNativeMethod(kClass, "RescueActor", &RescueActor, std::nullopt, false);
		vm->BindNativeMethod(kClass, "PacifyActor", &PacifyActor, std::nullopt, false);
		vm->BindNativeMethod(kClass, "ReleaseActor", &ReleaseActor, std::nullopt, false);
		vm->BindNativeMethod(kClass, "IsDefeated", &IsDefeated, std::nullopt, false);
		vm->BindNativeMethod(kClass, "IsPacified", &IsPacified, std::nullopt, false);
		vm->BindNativeMethod(kClass, "GetDefeated", &GetDefeated, std::nullopt, false);
		vm->BindNativeMethod(kClass, "DisableProcessing", &DisableProcessing, std::nullopt, false);
		vm->BindNativeMethod(kClass, "IsProcessingDisabled", &IsProcessingDisabled, std::nullopt, false);

		g_bound = true;
		REX::INFO("[OSFDefeat] Papyrus API bound");
	}
}
