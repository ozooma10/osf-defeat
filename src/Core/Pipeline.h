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
	// Trigger sink: called on the game thread when an actor goes down.
	void OnDown(const Trigger::DownEvent& a_event);

	// Re-establish a defeated actor loaded from a co-save (re-applies state + beat).
	void RestoreDefeated(FormID a_id, float a_registeredDay, FormID a_aggressor);
}
