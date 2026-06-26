#pragma once

#include "pch.h"

// =============================================================================
// Engine — the low-level, version-specific RE'd primitives. Everything that pokes
// raw engine memory or relies on a reverse-engineered address lives here, isolated
// from the rest of the core so a game-build update only touches this file.
//
// All of these were validated in the OSF RE sandbox on Starfield 1.16.244.
// =============================================================================
namespace OSFDefeat::Engine
{
	// Flip the global combat-AI gate the `tcai` console command toggles. true ->
	// every NPC breaks off combat; false -> normal. (Player-scope suppression; the
	// per-actor equivalent for NPC defeat is StopCombatAndAlarmOnActor, below.)
	void SetCombatAIDisabled(bool a_disabled);

	// Cached "Health" ActorValueInfo, or nullptr if forms aren't loaded yet.
	[[nodiscard]] RE::ActorValueInfo* HealthAV();

	// The ActorValueOwner subobject (CommonLibSF doesn't model it as an Actor base).
	[[nodiscard]] RE::ActorValueOwner* AVOwnerOf(RE::Actor* a_actor);

	[[nodiscard]] float GetHealth(RE::Actor* a_actor);

	// Heal to permanent max -> the engine exits bleedout and plays get-up.
	void RestoreHealthToMax(RE::Actor* a_actor);

	// Push current Health down to ~0 so an essential/protected actor drops into
	// bleedout. Used by the (best-effort) NPC defeat path.
	void DriveHealthFloor(RE::Actor* a_actor);

	// Make the player base (TESNPC) Essential so a lethal hit routes to bleedout.
	// Idempotent; cheap to call every frame.
	void EnsureEssential(RE::PlayerCharacter* a_player);

	// Set/clear the *instance* Essential flag (boolFlags 1<<18) on one Actor without
	// touching its shared base. Investigation hypothesis (1): does this route that
	// one instance's lethal hits to bleedout, leaving the base/other instances alone?
	void SetInstanceEssential(RE::Actor* a_actor, bool a_essential);

	// Read the per-instance "in bleedout animation" flag (boolFlags 1<<6) — the NPC
	// down signal (more reliable than HP polling for actors we don't fully control).
	[[nodiscard]] bool IsInBleedout(RE::Actor* a_actor);

	// --- Per-actor NPC defeat primitives (OSF RE-proven & live-validated 2026-06-25,
	// gameplay.defeat_damage_hook). All bound by AddrLib ID, so a game-build update only
	// re-anchors these IDs. SUPERSEDES the instance-essential / health-floor hypotheses above
	// (proven false: instance Essential does NOT route a lethal hit to bleedout — the death
	// path reads the BASE flag — and flooring a non-essential NPC's health just kills it).

	// ACTOR_LIFE_STATE values written by SetLifeState (bits 17-20 of [actor+0xF8]).
	enum LifeState : std::uint32_t
	{
		kAlive = 0,
		kBleedOut = 8,  // downs an arbitrary NPC into bleedout (live-proven); kEssentialDown(7) needs base-essential
	};

	// The real Actor::SetLifeState (AddrLib ID 100787 / vtable slot 0x16F). Drives the actor's
	// life state directly — base-form-agnostic, per-instance. kBleedOut downs it, kAlive revives.
	void SetLifeState(RE::Actor* a_actor, std::uint32_t a_lifeState);

	// Bethesda "ghost" (AddrLib ID 100873): true = invulnerable + AI de-prioritizes it as a target,
	// so the defeat STICKS through ongoing fire. Reversible.
	void SetGhost(RE::Actor* a_actor, bool a_ghost);

	// Manager-level combat removal (ProcessLists, AddrLib ID 103285): every other combatant DROPS
	// this actor as a target → they DISENGAGE. The per-actor lever plain StopCombat lacked (others
	// re-acquire after StopCombat but not after this); the core of console StopCombatAlarmOnActor.
	void StopCombatAndAlarmOnActor(RE::Actor* a_actor);

	// Resolve the actor's current combat target (handle @ Actor+0x298, "who this actor is
	// fighting") to an Actor*, via the engine handle manager (GetSmartPointer, .244 AddrLib ID
	// 35638 — already current in this copy's IDs.h, not the stale Skyrim 72432). Returns nullptr
	// if the actor isn't in combat or the handle is stale (the resolver age-checks → null, no
	// crash). HookTrigger's aggressor fallback when the damage call's a_ref is null (script/Papyrus
	// DamageValue path): unlike myKiller (only set when a kill COMMITS — which our clamp prevents,
	// so it reads empty for clamped victims, confirmed in-game 2026-06-25), the combat target is
	// populated throughout the fight. MUST run on the GAME THREAD: reads the handle table under the
	// manager's reader lock.
	[[nodiscard]] RE::Actor* ResolveCombatTarget(RE::Actor* a_actor);
}
