#include "pch.h"

#include "Core/Pipeline.h"

#include "Core/Defeat.h"
#include "Core/Validation.h"

namespace OSFDefeat::Pipeline
{
	namespace
	{
		// How long an actor stays down before the auto-revive (outcome policy).
		constexpr std::uint64_t kBeatMs = 8000;

		// Fixed real-time beat then auto-revive, marshalled back to the game thread.
		// (Matches the PoC. Game-time / save-aware recovery is a later refinement.)
		void ScheduleRecovery(FormID a_id, std::uint64_t a_ms)
		{
			std::thread([a_id, a_ms]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(a_ms));
				if (const auto* const tasks = SFSE::GetTaskInterface()) {
					tasks->AddTask([a_id]() {
						if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(a_id)) {
							Defeat::RescueActor(actor, true);
						}
					});
				}
			}).detach();
		}
	}

	void OnDown(const Trigger::DownEvent& a_event)
	{
		auto* const victim = a_event.victim;
		if (!victim) {
			return;
		}
		if (!Validation::CanProcess() || !Validation::ValidatePair(victim, a_event.aggressor)) {
			return;
		}

		// TODO (Decision layer): classify the encounter (hostile / crew / civilian /
		// security / bystander) and select an outcome. Scaffold = single default.
		if (!Defeat::DefeatActor(victim, a_event.aggressor)) {
			return;
		}

		// Default outcome: auto-recover after the beat (PoC behaviour).
		ScheduleRecovery(victim->GetFormID(), kBeatMs);
	}

	void RestoreDefeated(FormID a_id, float a_registeredDay, FormID a_aggressor)
	{
		Defeat::RestoreFromSave(a_id, a_registeredDay, a_aggressor);
		ScheduleRecovery(a_id, kBeatMs);
	}
}
