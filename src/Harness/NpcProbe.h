#pragma once

#include "pch.h"

// =============================================================================
// NpcProbe — a live, in-game validation harness for the per-actor NPC defeat
// investigation. NOT shipping behaviour; a switchable diagnostic.
//
// Each frame it auto-primes hostile, in-combat NPCs with the *instance* Essential
// flag (boolFlags 1<<18) and watches their boolFlags transitions, logging to
// OSFDefeat.log (tagged "[probe]"):
//
//   (1) alive -> bleedout   instance-essential routed the lethal hit  => (1) HOLDS
//       alive -> dead       instance-essential did NOT prevent death  => (1) FAILS
//   (2) defeated -> dead    a downed NPC died under continued damage  => (2) FAILS
//
// When an NPC enters bleedout the probe runs the real Defeat::DefeatActor verb on
// it (instance-essential hold path), so this also exercises the NPC verb end to
// end. Downed NPCs are intentionally left down (no auto-recovery) so you can keep
// firing to stress-test (2). Toggle off to restore normal lethality.
// =============================================================================
namespace OSFDefeat::Harness
{
	class NpcProbe
	{
	public:
		// Default on: this is the active investigation. Flip to compile-quiet later.
		static constexpr bool kDefaultEnabled = true;

		static void               Install();  // per-frame task; call once on the game thread
		static void               SetEnabled(bool a_enabled);
		[[nodiscard]] static bool IsEnabled();
	};
}
