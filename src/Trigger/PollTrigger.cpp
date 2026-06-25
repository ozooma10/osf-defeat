#include "pch.h"

#include "Trigger/PollTrigger.h"

#include "Core/Defeat.h"
#include "Engine/EngineFns.h"

namespace OSFDefeat::Trigger
{
	namespace
	{
		// Health at/below this => an essential actor is in bleedout (live: ~ -30).
		constexpr float kDownThreshold = 0.0f;
		// Re-arm only once Health climbs back above this; guards a load-time
		// transient (Health momentarily 0) and an immediate post-revive re-trigger.
		constexpr float kArmThreshold = 1.0f;
	}

	void PollTrigger::Install()
	{
		const auto* const tasks = SFSE::GetTaskInterface();
		if (!tasks) {
			REX::WARN("[OSFDefeat] no SFSE TaskInterface - poll trigger disabled");
			return;
		}
		tasks->AddPermanentTask([this]() { Poll(); });
		REX::INFO("[OSFDefeat] PollTrigger installed (per-frame)");
	}

	void PollTrigger::Poll()
	{
		auto* const player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;  // main menu / loading
		}
		Engine::EnsureEssential(player);  // re-applies after loads

		if (!Engine::HealthAV()) {
			return;  // forms not ready yet
		}
		const float  hp = Engine::GetHealth(player);
		const FormID id = player->GetFormID();

		if (hp > kArmThreshold) {
			_armed.insert(id);  // healthy -> ready to detect the next down
		}

		if (Defeat::IsDefeated(player)) {
			return;  // already down; recovery is the pipeline's job
		}

		if (_armed.contains(id) && hp <= kDownThreshold) {
			_armed.erase(id);
			REX::INFO("[OSFDefeat] poll: player DOWN (hp={:.1f})", hp);
			if (_sink) {
				_sink(DownEvent{ player, nullptr });  // poll can't attribute an aggressor
			}
		}
	}
}
