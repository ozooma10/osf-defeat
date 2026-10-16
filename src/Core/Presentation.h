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
	// kPostPostDataLoad (OSF marks itself ready at kPostDataLoad). If unavailable, the cache stays
	// null and scene presentation no-ops for the session.
	void AcquireSceneApi();

	// Put the player into the down scene: cower pose + locked (can't move/act) + free mouse-orbit
	// camera, with advance/end revoked and no armor strip. Calls the native StartScene; if OSF
	// Animation isn't loaded it logs a WARN and no-ops (the framework down-state still holds the
	// player, just unposed). Game thread.
	void ShowPlayerDown();

	// End the player's down scene (native StopSceneForActor) — OSF Animation owns pose/lock/camera
	// restoration through its undo ledger and queued service cleanup. Game thread.
	void ClearPlayerDown();

	// Restore presentation if the player remains defeated without an OSF scene. Called from the
	// recovery tick: once no scene has held the player for ~1.5s, re-runs ShowPlayerDown. The delay
	// allows scene restoration and content hand-back (scene ended, RecoverActor imminent) time
	// to finish. Failed starts are retried after another full delay. Game thread.
	void ReassertPlayerDown();

	// Clear session-only debounce state during persistence revert. Does not touch an engine scene;
	// the load pipeline owns scene teardown and the restored defeat will reassert if needed.
	void ResetRuntimeState();
}
