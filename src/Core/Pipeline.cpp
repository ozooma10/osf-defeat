#include "pch.h"

#include "Core/Pipeline.h"

#include "Core/Defeat.h"
#include "Core/DefeatRegistry.h"
#include "Core/Validation.h"

namespace OSFDefeat::Pipeline
{
	namespace
	{
		// The default recovery beat, in REAL seconds. It is converted to a game-time span (via the
		// live timescale) and stored as an absolute game-day deadline, so the wait: pauses when the
		// game pauses, fast-forwards through sleep/wait, and survives save/load — yet still feels
		// like ~this many seconds during normal play. (Outcome policy; Phase 3 makes it per-outcome.)
		constexpr float kBeatRealSeconds = 8.0f;

		// Seconds in a day, for the real-seconds -> game-days conversion.
		constexpr float kSecondsPerDay = 86400.0f;

		// How often the recovery tick scans the registry. This is a REAL-time throttle on the scan
		// cadence only; the deadline it checks is game-time. 4x/sec is far finer than the beat.
		constexpr std::uint64_t kTickIntervalMs = 250;

		[[nodiscard]] float NowDay()
		{
			const auto* const cal = RE::Calendar::GetSingleton();
			return cal ? cal->GetDaysPassedExact() : 0.0f;
		}

		// Game-thread recovery tick (registered as a permanent SFSE task). Revives every defeated
		// actor whose absolute game-time deadline has passed. Because the deadline is game-time,
		// this naturally stalls while the game is paused (NowDay frozen) and trips through any
		// sleep/wait the player does. Replaces the old per-defeat detached std::thread real-time
		// beat (which ignored game-time and was not save/load-safe).
		void TickRecovery()
		{
			static std::uint64_t s_lastTickMs = 0;
			const auto           nowMs = ::GetTickCount64();
			if (nowMs - s_lastTickMs < kTickIntervalMs) {
				return;
			}
			s_lastTickMs = nowMs;

			const float today = NowDay();
			// Snapshot() copies the ids, so rescuing (which erases from the registry) mid-loop is safe.
			for (const auto id : DefeatRegistry::Snapshot()) {
				const auto data = DefeatRegistry::Get(id);
				if (!data || data->state.load() != DefeatState::Defeated) {
					continue;  // being applied / already rescuing
				}
				const float deadline = data->recoverAtDay;
				if (deadline > 0.0f && today >= deadline) {  // <=0 = held indefinitely (Phase 3)
					if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
						Defeat::RescueActor(actor, true);  // removes it from the registry
					}
				}
			}
		}

		// Default outcome: auto-recover a beat from now. Stamps the victim's absolute game-time
		// deadline; the tick performs the actual revive. No-op if the victim isn't registered.
		void ScheduleRecovery(FormID a_id)
		{
			if (auto data = DefeatRegistry::Get(a_id)) {
				data->recoverAtDay = NowDay() + RecoverySpanDays();
			}
		}
	}

	float RecoverySpanDays()
	{
		const auto* const cal = RE::Calendar::GetSingleton();
		// timeScale = game-minutes per real-minute (== game-seconds per real-second). So
		// gameSeconds = realSeconds * timeScale, and gameDays = gameSeconds / 86400. Fallback 20
		// (the franchise default) if the calendar/global isn't ready.
		const float timeScale = (cal && cal->timeScale) ? cal->GetTimeScale() : 20.0f;
		return (kBeatRealSeconds * timeScale) / kSecondsPerDay;
	}

	void Install()
	{
		if (const auto* const tasks = SFSE::GetTaskInterface()) {
			tasks->AddPermanentTask([]() { TickRecovery(); });
			REX::INFO("[OSFDefeat] recovery tick installed ({:.1f}s game-time beat)", kBeatRealSeconds);
		} else {
			REX::WARN("[OSFDefeat] no SFSE TaskInterface - recovery tick disabled (actors stay down)");
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

		// Default outcome: auto-recover after the beat (game-time deadline; the tick revives).
		ScheduleRecovery(victim->GetFormID());
	}

	void RestoreDefeated(FormID a_id, float a_registeredDay, FormID a_aggressor, float a_recoverAtDay)
	{
		// Restore with the deadline FROM the save (absolute game-time): loading resumes the
		// countdown instead of restarting it. The recovery tick picks it up — no re-schedule.
		Defeat::RestoreFromSave(a_id, a_registeredDay, a_aggressor, a_recoverAtDay);
	}
}
