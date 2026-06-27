#pragma once

#include "pch.h"

#include "Trigger/ITrigger.h"

namespace OSFDefeat::Trigger
{
	// Trigger impl #2: the RE'd pre-death damage hook (the thing PollTrigger can't do).
	//
	// Entry-hooks ActorValueOwner::ModActorValue (slot-6 impl, AddrLib ID 101587) — the
	// universal Health-write chokepoint. On a would-be-lethal Health hit to a defeat
	// candidate it CLAMPS the damage delta so post-hit Health lands at a small floor (never
	// <= 0), so the engine's downstream kill path never fires (Acheron-style: clamp the
	// lethal write, don't divert the lifestate, don't resurrect). It reads the aggressor
	// from the hit and queues an attributed DownEvent for the game thread, where the existing
	// pipeline applies the held-bleedout defeat recipe.
	//
	// Scoped to NPCs: an NPC takes a lethal hit and dies in one frame, with no bleedout
	// window for a poll to catch — this hook is the only way to defeat it and attribute the
	// kill. The PLAYER stays on PollTrigger (its proven base-Essential -> engine-bleedout
	// path; clamping the player's HP to a floor would suppress that bleedout entirely).
	class HookTrigger final : public ITrigger
	{
	public:
		void SetSink(Sink a_sink) override { _sink = std::move(a_sink); }
		void Install() override;

	private:
		Sink _sink;
	};

	// Player surrender trigger configuration (free functions; the hook reads this on each hit). The
	// hook downs the player — firing the normal defeat pipeline + OnDefeat — when an in-combat Health
	// hit leaves them below this FRACTION of max health [0..1]. 0 = disabled (death-alternative only);
	// OFF by default. Exposed to consumers via IVDefeat3::SetPlayerSurrenderThreshold.
	void                SetPlayerSurrenderFraction(float a_fraction);
	[[nodiscard]] float GetPlayerSurrenderFraction();
}
