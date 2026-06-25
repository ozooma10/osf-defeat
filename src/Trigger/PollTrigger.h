#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"
#include "Trigger/ITrigger.h"

namespace OSFDefeat::Trigger
{
	// Trigger impl #1: the validated PoC. A per-frame SFSE task keeps the player
	// essential and polls Health; an essential actor reading Health <= 0 is in
	// bleedout, which no living actor ever is. Emits a DownEvent on that transition.
	//
	// Hook-free and player-scoped today; a future HookTrigger implements the same
	// ITrigger and can additionally attribute aggressors and drive NPC downs.
	class PollTrigger final : public ITrigger
	{
	public:
		void SetSink(Sink a_sink) override { _sink = std::move(a_sink); }
		void Install() override;

	private:
		void Poll();

		Sink              _sink;
		std::set<FormID>  _armed;  // actors seen alive since their last down (re-arm guard)
	};
}
