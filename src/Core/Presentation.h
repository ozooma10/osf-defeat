#pragma once

#include "pch.h"

namespace OSFDefeat::Presentation
{
	// The player-down presentation is DEFERRED to OSF Animation's scene system, driven through its
	// native C++ API (src/External/OSFSceneAPI.h) rather than the Papyrus VM. OSF Defeat ships the
	// scene def (dist/OSF/osfdefeat.osf.json -> Data/OSF) — a bare vanilla-cower clip — and forces the
	// down-state presentation overrides (lock on, advance/end off, orbit camera, no strip) per-start
	// from C++ via OSFStartOptions. OSF Animation owns the pose, the input lock, the orbit camera, and
	// the undo-ledger cleanup. Nothing here duplicates that.
	inline constexpr const char* kDownSceneId = "osf.defeat.cower";

	// Fetch + cache OSF Animation's native scene API (OSF::API::RequestSceneAPI), once. Call at SFSE
	// kPostPostDataLoad (OSF marks itself ready at kPostDataLoad). A too-early/absent OSF leaves the
	// cache null; ShowPlayerDown lazy-retries once before giving up. Read-only; thread-agnostic.
	void AcquireSceneApi();

	// Put the player into the down scene: cower pose + locked (can't move/act) + free mouse-orbit
	// camera, with advance/end revoked and no armor strip. Calls the native StartScene; if OSF
	// Animation isn't loaded it logs a WARN and no-ops (the framework down-state still holds the
	// player, just unposed). Game thread.
	void ShowPlayerDown();

	// End the player's down scene (native StopSceneForActor) — reverses pose/lock/camera via OSF
	// Animation's undo ledger. Game thread.
	void ClearPlayerDown();

	// Self-heal for the down presentation. The framework down-state survives a save-load (co-save)
	// and a cell change, but the OSF scene that poses + locks the player dies with both (OSF
	// Animation drops every scene on save-load, plus a late TESLoadGameEvent StopAll backstop) —
	// leaving the player held in raw kBleedOut: frozen, unposed, no orbit camera. Called from the
	// recovery tick while the player is registered defeated: once no scene has held the player for
	// ~1.5s, re-runs ShowPlayerDown. The debounce keeps the re-start clear of OSF's own post-load
	// teardown and of the content hand-back window (scene ended, RecoverActor imminent); if a race
	// is still lost, the next tick re-asserts again. Game thread.
	void ReassertPlayerDown();

	// Clear session-only debounce state during persistence revert. Does not touch an engine scene;
	// the load pipeline owns scene teardown and the restored defeat will reassert if needed.
	void ResetRuntimeState();

	// Low-level player-AI-driven toggle (Game.SetPlayerAIDriven via the VM). Used defensively by
	// ClearPlayerDown to clear any stale AI-driven flag on recovery. Unrelated to the scene API.
	void SetPlayerAIDriven(bool a_driven);
}
