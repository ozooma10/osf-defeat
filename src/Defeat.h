#pragma once

namespace OSFDefeat
{
	// Installed as an SFSE permanent (per-frame) task. Drives the whole framework:
	// keeps the player Essential, detects bleedout, suppresses combat for a beat,
	// then revives the player. Runs on the game thread only.
	void OnGameFrame();
}
