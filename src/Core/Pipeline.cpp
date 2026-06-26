#include "pch.h"

#include "Core/Pipeline.h"

#include "Core/Consequence.h"
#include "Core/Decision.h"
#include "Core/Defeat.h"
#include "Core/DefeatRegistry.h"
#include "Core/Validation.h"
#include "Engine/EngineFns.h"

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

		// Convert a REAL-seconds duration into game-days using the live timescale, so a span pauses
		// with the game and scales with sleep/wait yet still feels like that many real seconds during
		// normal play. timeScale = game-minutes per real-minute (== game-seconds per real-second);
		// gameSeconds = realSeconds * timeScale, gameDays = / 86400. Fallback 20 (franchise default).
		[[nodiscard]] float RealSecondsToGameDays(float a_realSeconds)
		{
			const auto* const cal = RE::Calendar::GetSingleton();
			const float       timeScale = (cal && cal->timeScale) ? cal->GetTimeScale() : 20.0f;
			return (a_realSeconds * timeScale) / kSecondsPerDay;
		}

		// Game-thread recovery tick (registered as a permanent SFSE task). Three jobs per defeated
		// actor: (1) auto-revive once its game-time deadline passes; (2) drop orphans — an expired
		// entry whose actor no longer resolves can never be rescued, so remove it (plugs a registry
		// leak that also bloats the co-save); (3) re-assert the down state if it drifted — a
		// defeated NPC whose 3D reloaded loses its bleedout/ghost, so poll for that and re-apply
		// (the poll-first answer to the 3D-reload gap; a Load3D hook is the future precise version).
		// Game-time deadlines mean (1)/(2) stall while paused and trip through sleep/wait.
		void TickRecovery()
		{
			static std::uint64_t s_lastTickMs = 0;
			const auto           nowMs = ::GetTickCount64();
			if (nowMs - s_lastTickMs < kTickIntervalMs) {
				return;
			}
			s_lastTickMs = nowMs;

			const float today = NowDay();
			// Snapshot() copies the ids, so removing/rescuing (which erase from the registry)
			// mid-loop is safe.
			for (const auto id : DefeatRegistry::Snapshot()) {
				const auto data = DefeatRegistry::Get(id);
				if (!data || data->state.load() != DefeatState::Defeated) {
					continue;  // being applied / already rescuing
				}
				auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id);
				const float deadline = data->recoverAtDay;
				const bool  expired = deadline > 0.0f && today >= deadline;  // <=0 = held indefinitely

				if (expired) {
					if (actor) {
						Defeat::RescueActor(actor, true);  // normal recovery (removes from registry)
					} else {
						DefeatRegistry::Remove(id);  // orphan: gone before recovery -> drop it
					}
					continue;
				}

				// Still held: keep the defeat asserted across 3D reloads. If a defeated NPC's life
				// state is no longer bleedout, it drifted (reload reset it) -> re-apply. The guard
				// means the common case (still kBleedOut) does nothing but a cheap read.
				if (actor && !actor->IsPlayerRef() && Engine::GetLifeState(actor) != Engine::kBleedOut) {
					Defeat::ReapplyDownState(actor);
				}
			}
		}

		// Apply the selected outcome's recovery policy by stamping the victim's absolute game-time
		// deadline; the tick performs the actual revive. `hold` => no deadline (recoverAtDay <= 0,
		// held until an explicit rescue). No-op if the victim isn't registered.
		void ApplyOutcomeRecovery(FormID a_id, const Decision::Outcome& a_outcome)
		{
			auto data = DefeatRegistry::Get(a_id);
			if (!data) {
				return;
			}
			if (a_outcome.hold) {
				data->recoverAtDay = 0.0f;  // held; the tick skips deadlines <= 0
				return;
			}
			const float spanDays = a_outcome.recoverySeconds >= 0.0f
									 ? RealSecondsToGameDays(a_outcome.recoverySeconds)
									 : RecoverySpanDays();
			data->recoverAtDay = NowDay() + spanDays;
		}
	}

	float RecoverySpanDays()
	{
		return RealSecondsToGameDays(kBeatRealSeconds);
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

		// Decision layer: classify the encounter, then select the outcome (recovery policy today;
		// pluggable / data-driven consequences later).
		const auto encounterClass = Decision::Classify(victim, a_event.aggressor);
		const auto outcome = Decision::SelectOutcome(encounterClass, victim, a_event.aggressor);

		if (!Defeat::DefeatActor(victim, a_event.aggressor)) {
			return;
		}

		ApplyOutcomeRecovery(victim->GetFormID(), outcome);
		REX::INFO("[OSFDefeat] outcome for {:08X}: class={}, {}", victim->GetFormID(),
			Decision::ClassName(encounterClass), outcome.hold ? "held" : "auto-recover after beat");

		// Fire the matched data rule's authored consequences (event broadcasts / log effects). Runs
		// after the defeat is applied + the kOnDefeat broadcast, so consumers see the defeat first.
		const OSFDefeatAPI::DefeatContext ctx{
			victim->GetFormID(),
			a_event.aggressor ? a_event.aggressor->GetFormID() : 0u,
			static_cast<std::uint32_t>(encounterClass),
		};
		Consequence::Fire(ctx);
	}

	void RestoreDefeated(FormID a_id, float a_registeredDay, FormID a_aggressor, float a_recoverAtDay)
	{
		// Restore with the deadline FROM the save (absolute game-time): loading resumes the
		// countdown instead of restarting it. The recovery tick picks it up — no re-schedule.
		Defeat::RestoreFromSave(a_id, a_registeredDay, a_aggressor, a_recoverAtDay);
	}
}
