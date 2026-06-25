#include "pch.h"

#include "Defeat.h"

#include <Windows.h>  // GetTickCount64

// =============================================================================
// OSF Defeat — native defeat / death-alternative framework (Skyrim "Acheron" analog)
//
// The full loop, all reverse-engineered + live-validated on Starfield 1.16.244 in
// the OSF RE sandbox (module `gameplay.defeat_damage_hook`):
//
//   lethal hit  -> player does NOT die, drops into BLEEDOUT
//               -> nearby enemies disengage (combat suppressed)
//               -> short "defeat beat"
//               -> player is REVIVED (gets back up) and combat resumes
//
// Each piece uses the engine's own machinery, so this mod is HOOK-FREE — it only
// sets engine flags / writes one engine global from a per-frame task:
//
//   * Don't die        : set the player's base (TESNPC) Essential -> the engine's
//                        own death path routes a lethal hit into bleedout.
//   * Enemies disengage: flip the global combat-AI gate via the `tcai` lever, wrapped
//                        as RE::Console::SetCombatAIProcessing. Per-actor StopCombat was
//                        tried and FAILS (the AI re-acquires the player next tick); the
//                        global gate suppresses target acquisition itself.
//   * Get back up      : the essential player has NO natural bleedout recovery (it
//                        stays frozen at a negative-Health floor), so we actively
//                        revive it with RestoreActorValue(Health, max).
//
// Detection is pure Health polling (no damage hook needed): an essential player in
// bleedout reads Health <= 0 (live: ~ -30), which no living actor ever does.
// =============================================================================
namespace OSFDefeat
{
	namespace
	{
		// --- tuning ---------------------------------------------------------------

		// Health at/below this => the essential player is in bleedout (live: ~ -30;
		// no living actor sits at <= 0).
		constexpr float kDownThreshold = 0.0f;
		// Re-arm the detector only once Health climbs back above this. Prevents a
		// load-time transient (Health momentarily 0) and an immediate re-trigger
		// right after a revive from firing a spurious defeat.
		constexpr float kArmThreshold = 1.0f;
		// How long the player stays down (enemies suppressed) before the auto-revive.
		constexpr std::uint64_t kBeatMs = 8000;

		// The engine's ActorValueOwner subobject sits at Actor+0x70 on 1.16.244
		// (CommonLibSF does not model it as an Actor base); reach it explicitly.
		constexpr std::ptrdiff_t kActorValueOwnerOffset = 0x70;

		// --- state (game thread only; the SFSE permanent task is single-threaded) ---

		bool          g_armed = false;   // ready to detect a down (seen alive since last defeat)
		bool          g_down = false;    // currently in a defeat beat
		std::uint64_t g_beatStart = 0;   // GetTickCount64 when the beat began

		// --- helpers --------------------------------------------------------------

		[[nodiscard]] RE::ActorValueInfo* HealthAV()
		{
			static RE::ActorValueInfo* cached =
				RE::TESForm::LookupByEditorID<RE::ActorValueInfo>("Health");
			return cached;
		}

		[[nodiscard]] RE::ActorValueOwner* AVOwnerOf(RE::Actor* a_actor)
		{
			return reinterpret_cast<RE::ActorValueOwner*>(
				reinterpret_cast<std::byte*>(a_actor) + kActorValueOwnerOffset);
		}

		void EnsureEssential(RE::PlayerCharacter* a_player)
		{
			auto* const npc = a_player->GetNPC();
			if (npc && !npc->IsEssential()) {
				npc->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kEssential);
				REX::INFO("[OSFDefeat] player base {:08X} set Essential", npc->GetFormID());
			}
		}

		// Bring the downed player back up: restore Health to full so the engine exits
		// bleedout and plays get-up. RestoreActorValue (AV-owner vtable slot 0x09) takes
		// no source ref and is safe; it clamps to the permanent max, so passing maxHp
		// heals to full. This revives a *bleedout* (not flagged-dead) actor.
		void RevivePlayer(RE::PlayerCharacter* a_player, RE::ActorValueInfo* a_healthAV)
		{
			auto* const avOwner = AVOwnerOf(a_player);
			const float maxHp = avOwner->GetPermanentActorValue(*a_healthAV);
			const float curHp = avOwner->GetActorValue(*a_healthAV);
			if (maxHp > 0.0f) {
				avOwner->RestoreActorValue(*a_healthAV, maxHp);  // clamp to max -> full heal
				REX::INFO("[OSFDefeat] revive: Health {:.1f} -> ~{:.0f}", curHp, maxHp);
			}
		}
	}

	void OnGameFrame()
	{
		auto* const player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;  // main menu / loading - no player yet
		}
		EnsureEssential(player);  // keep the player essential (re-applies after loads)

		auto* const healthAV = HealthAV();
		if (!healthAV) {
			return;  // forms not ready yet
		}
		const float hp = AVOwnerOf(player)->GetActorValue(*healthAV);
		const auto  now = ::GetTickCount64();

		if (hp > kArmThreshold) {
			g_armed = true;  // healthy -> ready to detect the next down
		}

		if (!g_down) {
			if (g_armed && hp <= kDownThreshold) {  // entered essential bleedout
				g_down = true;
				g_armed = false;
				g_beatStart = now;
				RE::Console::SetCombatAIProcessing(false);  // suppress all combat AI
				REX::INFO("[OSFDefeat] player DOWN (hp={:.1f}) -> combat suppressed", hp);
			}
		} else if (now - g_beatStart > kBeatMs) {  // defeat beat over
			RevivePlayer(player, healthAV);
			RE::Console::SetCombatAIProcessing(true);  // restore combat AI
			g_down = false;
			REX::INFO("[OSFDefeat] defeat beat over -> player revived + combat restored");
		}
	}
}
