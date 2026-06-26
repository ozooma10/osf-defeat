#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"
#include "Trigger/ITrigger.h"

// =============================================================================
// Pipeline — the orchestrator. A trigger reports a down; the pipeline validates,
// (eventually) classifies the encounter, applies the defeat state, and decides
// the outcome. For the scaffold the only outcome is the PoC behaviour: auto-revive
// after a fixed beat. The Decision/Resolution layers slot in here later.
// =============================================================================
namespace OSFDefeat::Pipeline
{
	// Register the game-thread recovery tick. Call once from InstallCore (game thread).
	void Install();

	// Trigger sink: called on the game thread when an actor goes down.
	void OnDown(const Trigger::DownEvent& a_event);

	// Re-establish a defeated actor loaded from a co-save. The recovery deadline comes from the
	// save (absolute game-time), so loading RESUMES the countdown rather than restarting it.
	void RestoreDefeated(FormID a_id, float a_registeredDay, FormID a_aggressor, float a_recoverAtDay);

	// The default recovery beat as a span of game-days (a real-seconds beat scaled by the live
	// timescale). Exposed so persistence can synthesize a deadline for pre-deadline (v1) saves.
	[[nodiscard]] float RecoverySpanDays();
}
