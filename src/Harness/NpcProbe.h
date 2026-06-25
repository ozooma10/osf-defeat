#pragma once

#include "pch.h"

// =============================================================================
// NpcProbe — a live, in-game smoke test for the per-actor NPC defeat verb. NOT
// shipping behaviour; an opt-in diagnostic.
//
// The original instance-essential investigation is RESOLVED: instance Essential does
// NOT route a lethal hit to bleedout (the death path reads the BASE flag). The proven
// recipe is bleedout life state + ghost (invulnerable) + manager-level combat removal,
// now in Defeat::DefeatActor. This harness exercises that verb end to end.
//
// When enabled, each frame it defeats the actor under the crosshair via the real
// Defeat::DefeatActor verb and watches it (logging to OSFDefeat.log, tagged "[probe]"):
// a defeated NPC must stay down and must NOT die under continued fire (ghost holds).
// Downed NPCs are left down (no auto-recovery) so you can keep firing to stress-test.
// Toggle off to restore normal behaviour.
// =============================================================================
namespace OSFDefeat::Harness
{
	class NpcProbe
	{
	public:
		// Opt-in: the investigation it served is resolved, and it auto-defeats on crosshair.
		static constexpr bool kDefaultEnabled = false;

		static void               Install();  // per-frame task; call once on the game thread
		static void               SetEnabled(bool a_enabled);
		[[nodiscard]] static bool IsEnabled();
	};
}
