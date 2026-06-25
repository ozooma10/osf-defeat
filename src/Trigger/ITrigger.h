#pragma once

#include "pch.h"

namespace OSFDefeat::Trigger
{
	// A single "this actor just went down" signal produced by a trigger and handed
	// to the pipeline. The aggressor is whoever caused the down, or null when the
	// trigger can't attribute it (e.g. the poll trigger has no hit data).
	struct DownEvent
	{
		RE::Actor* victim;
		RE::Actor* aggressor;
	};

	// The pluggable trigger seam. The core depends only on this interface, so the
	// detection strategy can change (poll today; a real damage/hit hook later)
	// without touching the state machine, pipeline, or API.
	class ITrigger
	{
	public:
		using Sink = std::function<void(const DownEvent&)>;

		virtual ~ITrigger() = default;

		// Where detected downs are delivered. Set before Install().
		virtual void SetSink(Sink a_sink) = 0;

		// Begin producing DownEvents (install the poll task / hooks). Must run on
		// the game thread.
		virtual void Install() = 0;
	};
}
