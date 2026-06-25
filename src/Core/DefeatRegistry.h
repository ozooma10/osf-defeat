#pragma once

#include "pch.h"

namespace OSFDefeat
{
	using FormID = std::uint32_t;

	enum class DefeatState
	{
		Defeating,  // state is being applied
		Defeated,   // fully down
		Rescuing    // recovery in progress
	};

	// Per-victim record. Held by shared_ptr because it carries an atomic (so it is
	// neither copyable nor movable) and callers may hold a handle across the lock.
	struct VictimData
	{
		VictimData() = default;
		VictimData(float a_registeredDay, FormID a_aggressor) :
			registeredDay(a_registeredDay), aggressor(a_aggressor) {}

		float                    registeredDay{ 0.0f };  // Calendar GetDaysPassedExact at defeat (persisted)
		FormID                   aggressor{ 0 };         // who downed this actor, or 0
		std::atomic<DefeatState> state{ DefeatState::Defeating };
	};

	// Thread-safe, actor-agnostic store of every actor currently in a defeat state.
	// This is the heart of the core: triggers feed it, verbs mutate engine state
	// around it, persistence (de)serializes it, and the API reads it.
	class DefeatRegistry
	{
	public:
		// Inserts a new victim. Returns the data, or nullptr if already present.
		static std::shared_ptr<VictimData> Add(FormID a_id, float a_registeredDay, FormID a_aggressor);
		static std::shared_ptr<VictimData> Get(FormID a_id);
		static bool                         Contains(FormID a_id);
		static bool                         Remove(FormID a_id);
		static std::vector<FormID>          Snapshot();
		static void                         Clear();

	private:
		static inline std::map<FormID, std::shared_ptr<VictimData>> _victims;
		static inline std::shared_mutex                             _m;
	};
}
