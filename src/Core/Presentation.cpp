#include "pch.h"

#include "Core/Presentation.h"

#include "External/OSFSceneAPI.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameVM.h"
#include "RE/I/IVirtualMachine.h"

namespace OSFDefeat::Presentation
{
	namespace
	{
		// Cached OSF Animation native scene API. Fetched once at kPostPostDataLoad (AcquireSceneApi),
		// with a single lazy retry on the first ShowPlayerDown if OSF wasn't ready yet. nullptr means
		// OSF Animation is absent / too old / not-ready: the scene presentation no-ops (the framework
		// down-state still holds the player, they just won't be posed).
		OSF::API::IOSFSceneAPI* g_sceneApi = nullptr;
		bool                    g_lazyRetried = false;

		// ReassertPlayerDown debounce: when the down scene was first observed missing ({} = not
		// missing). The scene must stay missing for a few recovery ticks before we restart it, so a
		// post-load re-start lands AFTER OSF Animation's TESLoadGameEvent backstop (late StopAll +
		// camera recovery) instead of racing it, and so a content hand-back window (scene already
		// stopped, RecoverActor imminent) never triggers a spurious cower.
		constexpr std::chrono::milliseconds        kReassertDelay{ 1500 };
		std::chrono::steady_clock::time_point      g_downSceneMissingSince{};

		// Return the cached API, performing exactly one lazy RequestSceneAPI retry if AcquireSceneApi
		// came up empty (OSF not yet ready at kPostPostDataLoad). After that single retry we stop
		// poking the module/symbol lookup and just no-op.
		[[nodiscard]] OSF::API::IOSFSceneAPI* EnsureSceneApi()
		{
			if (!g_sceneApi && !g_lazyRetried) {
				g_lazyRetried = true;
				g_sceneApi = OSF::API::RequestSceneAPI();
				if (g_sceneApi) {
					REX::INFO("[OSFDefeat] OSF Animation native scene API acquired on lazy retry");
				}
			}
			return g_sceneApi;
		}

		[[nodiscard]] RE::BSScript::IVirtualMachine* VM()
		{
			auto* const game = RE::GameVM::GetSingleton();
			return game ? game->GetVM() : nullptr;
		}

		[[nodiscard]] RE::Actor* Player()
		{
			return static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton());
		}
	}

	void AcquireSceneApi()
	{
		g_sceneApi = OSF::API::RequestSceneAPI();
		if (g_sceneApi) {
			std::uint32_t major = 0, minor = 0, patch = 0;
			g_sceneApi->GetPluginVersion(major, minor, patch);
			REX::INFO("[OSFDefeat] OSF Animation native scene API acquired (iface {:#x}, plugin {}.{}.{})",
				g_sceneApi->GetInterfaceVersion(), major, minor, patch);
		} else {
			REX::INFO("[OSFDefeat] OSF Animation native scene API not ready at kPostPostDataLoad "
					  "(absent/older/not-ready) - will lazy-retry on first player-down");
		}
	}

	void SetPlayerAIDriven(bool a_driven)
	{
		auto* const vm = VM();
		if (!vm) {
			return;
		}
		vm->DispatchStaticCall(
			"Game", "SetPlayerAIDriven",
			[a_driven](RE::BSScrapArray<RE::BSScript::Variable>& a_args) {
				a_args.resize(1);
				RE::BSScript::PackVariable(a_args[0], a_driven);
				return true;
			},
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>(), 0);
	}

	void ShowPlayerDown()
	{
		auto* const player = Player();
		if (!player) {
			return;
		}
		// PlayerCharacter can outlive its actor-base reference briefly while a save/load or world
		// transition rebuilds the reference. OSF's scene validation inspects that base, so defer the
		// presentation until ReassertPlayerDown's next debounced retry instead of passing a partially
		// initialized actor across the native API boundary.
		if (!player->GetBaseObject()) {
			REX::DEBUG("[OSFDefeat] player-down: actor base unavailable - deferring scene start");
			return;
		}

		auto* const api = EnsureSceneApi();
		if (!api) {
			REX::WARN("[OSFDefeat] player-down: OSF Animation native API unavailable - "
					  "player held by the framework down-state but not posed");
			return;
		}

		// Idempotency: StartScene returns 0 if the player is already in a scene, so end any prior one first (a stale cower from a previous defeat, an unrelated OSF scene, etc.).
		api->StopSceneForActor(player);

		// Force the down-state presentation from code (the def is a bare cower clip). Tri-states:
		// 1 = on, 0 = off, -1 = inherit.
		OSF::API::OSFStartOptions opts{};
		opts.lockPlayerMode    = 1;  // force movement/combat lock
		opts.stripMode         = 0;  // never strip armor on defeat
		opts.playerControlMode = 0;  // revoke advance/navigate/end — player can't escape the scene
		std::strncpy(opts.camera, "scene_orbit", sizeof(opts.camera) - 1);

		RE::Actor* actors[1]{ player };
		const std::int32_t handle = api->StartScene(actors, 1, kDownSceneId, opts);
		if (handle != 0) {
			REX::INFO("[OSFDefeat] player-down: OSF native StartScene('{}') -> handle {} "
					  "(cower + lock + orbit camera, no escape/strip)",
				kDownSceneId, handle);
		} else {
			REX::WARN("[OSFDefeat] player-down: OSF native StartScene('{}') failed (handle 0)", kDownSceneId);
		}
	}

	void ClearPlayerDown()
	{
		g_downSceneMissingSince = {};
		auto* const player = Player();
		if (!player) {
			return;
		}
		if (g_sceneApi) {
			g_sceneApi->StopSceneForActor(player);
		}
		SetPlayerAIDriven(false);  // defensive: clear any stale AI-driven flag left on the player
	}

	void ReassertPlayerDown()
	{
		auto* const player = Player();
		if (!player) {
			return;
		}
		auto* const api = EnsureSceneApi();
		if (!api) {
			return;  // OSF absent: ShowPlayerDown already warned at down time; nothing to heal
		}
		if (api->GetSceneForActor(player) != 0) {
			g_downSceneMissingSince = {};  // a scene holds the player (ours or content's) — healthy
			return;
		}

		const auto now = std::chrono::steady_clock::now();
		if (g_downSceneMissingSince == std::chrono::steady_clock::time_point{}) {
			g_downSceneMissingSince = now;
			return;
		}
		if (now - g_downSceneMissingSince < kReassertDelay) {
			return;
		}

		// Re-arm rather than clear-on-success: a failed StartScene (missing def, mid-teardown
		// actor) retries after another full delay instead of every tick.
		g_downSceneMissingSince = {};
		REX::INFO("[OSFDefeat] player-down scene missing (save-load / cell change / external stop) - re-asserting presentation");
		ShowPlayerDown();
	}

	void ResetRuntimeState()
	{
		g_downSceneMissingSince = {};
	}
}
