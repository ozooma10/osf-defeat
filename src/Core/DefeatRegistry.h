#pragma once

#include "pch.h"

namespace OSFDefeat
{
	using FormID = std::uint32_t;

	// How the actor went down (persisted; Papyrus GetDefeatCause / GetLastPlayerDefeatCause).
	// kScripted = the DefeatActor verb (surrender keys, capture, content-driven defeats), which
	// content can route distinctly from a Health-down. Values are API-stable.
	enum class DefeatCause : std::uint8_t
	{
		kNone = 0,     // not defeated / unknown
		kHealth = 1,   // lethal Health damage clamped
		kScripted = 2  // DefeatActor verb
	};

	// Engine properties OSF temporarily owns while an actor is down. Persisted so recovery after a
	// save/load restores the actor's actual prior state instead of imposing guessed defaults.
	struct ActorStateSnapshot
	{
		bool valid{ false };
		bool aiEnabled{ true };
		bool noBleedoutRecovery{ false };
		bool canSpeakToEssentialDown{ false };
		bool ghost{ false };
		bool actorEssential{ false };
		bool baseEssential{ false };
		RE::ACTOR_LIFE_STATE lifeState{ RE::ACTOR_LIFE_STATE::kAlive };
	};

	// Per-defeated-actor record. Held by shared_ptr because it carries an atomic (so it is
	// neither copyable nor movable) and callers may hold a handle across the lock.
	struct ActiveDefeat
	{
		ActiveDefeat() = default;
		ActiveDefeat(float a_registeredDay, FormID a_aggressor, float a_autoRecoverAtDay = 0.0f) :
			registeredDay(a_registeredDay), aggressor(a_aggressor), autoRecoverAtDay(a_autoRecoverAtDay) {}

		float                    registeredDay{ 0.0f };  // Calendar GetDaysPassedExact at defeat (persisted)
		std::atomic<float>       autoRecoverAtDay{ 0.0f };  // absolute game-day auto-recovery deadline; <=0 = recovery held (persisted)
		FormID                   aggressor{ 0 };         // who downed this actor, or 0
		std::atomic<DefeatCause> cause{ DefeatCause::kNone };  // how they went down (persisted); write-once at defeat
		ActorStateSnapshot       priorState;  // game-thread only; Save is serialized by RuntimeMutation

		// Defeat lease (persisted). A recovery hold (autoRecoverAtDay <= 0) may be leased by a
		// content consumer: leaseHolderId identifies it and leaseExpiresAtDay is the absolute
		// game-day watchdog deadline — if it lapses without an
		// extension, the recovery tick force-recovers and logs the defaulting holder. <= 0 = no
		// deadline armed (yet). The player's held entries are always auto-armed by the tick when
		// the watchdog is enabled; NPC entries only when explicitly leased (a held NPC captive is
		// a legitimate long-term state).
		std::atomic<float> leaseExpiresAtDay{ 0.0f };
		std::string        leaseHolderId;  // guarded by leaseLock: written on the game thread, but read
		mutable std::mutex leaseLock;      // from the VM and save threads — a torn string read is a crash

		[[nodiscard]] std::string CopyLeaseHolderId() const
		{
			std::scoped_lock lock{ leaseLock };
			return leaseHolderId;
		}
		void SetLeaseHolderId(std::string a_holderId)
		{
			std::scoped_lock lock{ leaseLock };
			leaseHolderId = std::move(a_holderId);
		}

		// registeredDay/aggressor are immutable after publication. The timer fields and cause are
		// atomic because OSF persistence snapshots can run off the game thread while verbs or the
		// recovery tick update them.
	};

	// Thread-safe, actor-agnostic store of every actor currently in a defeat state.
	// This is the heart of the core: triggers feed it, verbs mutate engine state
	// around it, persistence (de)serializes it, and the API reads it.
	class DefeatRegistry
	{
	public:
		// Inserts a new active defeat. Returns the data, or nullptr if already present.
		static std::shared_ptr<ActiveDefeat> Add(
			FormID a_id, float a_registeredDay, FormID a_aggressor, float a_autoRecoverAtDay = 0.0f);
		static std::shared_ptr<ActiveDefeat> Get(FormID a_id);
		static bool                         Contains(FormID a_id);
		static bool                         Remove(FormID a_id);
		static std::vector<FormID>          Snapshot();
		static void                         Clear();

	private:
		static inline std::map<FormID, std::shared_ptr<ActiveDefeat>> _activeDefeats;
		static inline std::shared_mutex                             _m;
	};
}
