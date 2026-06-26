#include "pch.h"

#include "Harness/Spike.h"

#include "RE/B/BSScriptUtil.h"    // PackVariable, BindNativeMethod template, deduction guides
#include "RE/G/GameVM.h"          // GameVM::GetSingleton()->GetVM()
#include "RE/I/IVirtualMachine.h"  // DispatchStaticCall
#include "RE/T/TESQuest.h"         // RE::TESQuest param/lookup

namespace OSFDefeat::Harness
{
	namespace
	{
		bool g_bound = false;

		// --- Spike A ----------------------------------------------------------
		// Papyrus: bool Function Ping(string asTag) global native
		//
		// Logs the calling thread, then queues a game-thread task (the same SFSE
		// TaskInterface marshaling APIServer uses) that does a game-thread-only read
		// and logs ITS thread. Two outcomes prove the seam:
		//   1. this native ran + returned a value to Papyrus  -> native bind + return marshaling works
		//   2. the task logged from the game thread            -> state-mutating calls can be marshaled
		bool Ping(std::monostate, RE::BSFixedString a_tag)
		{
			const std::uint32_t callerTid = ::GetCurrentThreadId();
			REX::INFO("[OSFSpike][A] Ping native called: tag='{}' callerTID={}", a_tag.c_str(), callerTid);

			const auto* const tasks = SFSE::GetTaskInterface();
			if (!tasks) {
				REX::WARN("[OSFSpike][A] no TaskInterface - game-thread marshaling unavailable");
				return false;  // the returned bool itself proves native->Papyrus return marshaling
			}
			tasks->AddTask([callerTid]() {
				const std::uint32_t taskTid = ::GetCurrentThreadId();
				auto* const         player = RE::PlayerCharacter::GetSingleton();  // game-thread-only read
				REX::INFO("[OSFSpike][A] marshaled task ran: taskTID={} callerTID={} differ={} player={:08X}",
					taskTid, callerTid, taskTid != callerTid, player ? player->GetFormID() : 0);
			});
			return true;
		}

		// --- Spike B ----------------------------------------------------------
		// Papyrus: bool Function RunHandoff(Quest akQuest, Actor akTarget) global native
		//
		// Receiving a Quest* AND an Actor* here already proves C++ can take arbitrary
		// game refs from content (how content would register its consequence quest).
		// Then, on the game thread, C++ pushes both refs back into content Papyrus via
		// DispatchStaticCall(OSFSpikeContent.Fill) — the content script does the actual
		// alias ForceRefTo (pure-C++ alias fill is unavailable in this CommonLibSF copy).
		bool RunHandoff(std::monostate, RE::TESQuest* a_quest, RE::Actor* a_target)
		{
			const std::uint32_t questId  = a_quest ? a_quest->GetFormID() : 0;
			const std::uint32_t targetId = a_target ? a_target->GetFormID() : 0;
			REX::INFO("[OSFSpike][B] RunHandoff native: quest={:08X} target={:08X} (C++ received both refs from Papyrus)",
				questId, targetId);
			if (!a_quest || !a_target) {
				return false;
			}

			const auto* const tasks = SFSE::GetTaskInterface();
			if (!tasks) {
				return false;
			}
			tasks->AddTask([questId, targetId]() {
				const auto vm = RE::GameVM::GetSingleton() ? RE::GameVM::GetSingleton()->GetVM() : nullptr;
				if (!vm) {
					REX::WARN("[OSFSpike][B] no VM on game thread");
					return;
				}
				// Arguments functor: the VM calls this to pack the call's params. (If
				// BSScrapArray lacks resize/operator[], use two push_back({}) instead.)
				const auto args = [questId, targetId](RE::BSScrapArray<RE::BSScript::Variable>& a_args) -> bool {
					auto* const q = RE::TESForm::LookupByID<RE::TESQuest>(questId);
					auto* const t = RE::TESForm::LookupByID<RE::Actor>(targetId);
					if (!q || !t) {
						return false;
					}
					a_args.resize(2);
					RE::BSScript::PackVariable(a_args[0], q);
					RE::BSScript::PackVariable(a_args[1], t);
					return true;
				};
				const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> noCallback{};
				const bool dispatched = vm->DispatchStaticCall("OSFSpikeContent", "Fill", args, noCallback, 0);
				REX::INFO("[OSFSpike][B] DispatchStaticCall(OSFSpikeContent.Fill) dispatched={} "
						  "- content script does the alias ForceRefTo; confirm in Papyrus.0.log",
					dispatched);
			});
			return true;
		}
	}

	void Spike::Register()
	{
		if (g_bound) {
			return;
		}
		const auto vm = RE::GameVM::GetSingleton() ? RE::GameVM::GetSingleton()->GetVM() : nullptr;
		if (!vm) {
			REX::WARN("[OSFSpike] VM not ready at registration - spikes NOT bound (try a later SFSE message)");
			return;
		}
		// Global functions tag the self slot with std::monostate (BSScriptUtil.h: concept static_tag).
		vm->BindNativeMethod("OSFSpike", "Ping", &Ping, std::nullopt, false);
		vm->BindNativeMethod("OSFSpike", "RunHandoff", &RunHandoff, std::nullopt, false);
		g_bound = true;
		REX::INFO("[OSFSpike] native globals bound: OSFSpike.Ping, OSFSpike.RunHandoff");
	}
}
