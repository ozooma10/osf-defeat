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
		VictimData(float a_registeredDay, FormID a_aggressor, float a_recoverAtDay = 0.0f) :
			registeredDay(a_registeredDay), aggressor(a_aggressor), recoverAtDay(a_recoverAtDay) {}

		float                    registeredDay{ 0.0f };  // Calendar GetDaysPassedExact at defeat (persisted)
		float                    recoverAtDay{ 0.0f };   // absolute game-day auto-recovery deadline; <=0 = held (persisted)
		FormID                   aggressor{ 0 };         // who downed this actor, or 0
		std::atomic<DefeatState> state{ DefeatState::Defeating };

		// Ownership lease (persisted). A held down-state (recoverAtDay <= 0) may be claimed by a
		// content consumer: leaseOwner names it (consequence name or ClaimDefeat token) and
		// leaseDeadlineDay is the absolute game-day watchdog deadline — if it lapses without an
		// extension, the recovery tick force-rescues and logs the defaulting owner. <= 0 = no
		// deadline armed (yet). The player's held entries are always auto-armed by the tick when
		// the watchdog is enabled; NPC entries only when explicitly leased (a held NPC captive is
		// a legitimate long-term state).
		float              leaseDeadlineDay{ 0.0f };
		std::string        leaseOwner;   // guarded by leaseLock: written on the game thread, but read
		mutable std::mutex leaseLock;    // from the VM (GetClaimOwner) and save threads — a torn string read is a crash, unlike the benign float races

		[[nodiscard]] std::string CopyLeaseOwner() const
		{
			std::scoped_lock lock{ leaseLock };
			return leaseOwner;
		}
		void SetLeaseOwner(std::string a_owner)
		{
			std::scoped_lock lock{ leaseLock };
			leaseOwner = std::move(a_owner);
		}

		// registeredDay/aggressor are write-once at construction; recoverAtDay and leaseDeadlineDay
		// are mutated only on the game thread (Pipeline outcome + verbs + recovery tick + co-save
		// Save) — no atomic needed.
	};

	// Thread-safe, actor-agnostic store of every actor currently in a defeat state.
	// This is the heart of the core: triggers feed it, verbs mutate engine state
	// around it, persistence (de)serializes it, and the API reads it.
	class DefeatRegistry
	{
	public:
		// Inserts a new victim. Returns the data, or nullptr if already present.
		static std::shared_ptr<VictimData> Add(
			FormID a_id, float a_registeredDay, FormID a_aggressor, float a_recoverAtDay = 0.0f);
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
