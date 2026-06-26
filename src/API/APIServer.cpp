#include "pch.h"

#include "API/APIServer.h"

#include "Core/Decision.h"
#include "Core/Defeat.h"
#include "Core/Pipeline.h"

namespace OSFDefeat::API
{
	namespace
	{
		// Fire-and-forget broadcast that does NOT log when there are zero subscribers.
		// CommonLibSF's MessagingInterface::Dispatch logs an ERROR whenever the underlying
		// SFSE dispatch returns false, and a broadcast (receiver = nullptr) returns false
		// precisely when no plugin is listening — the normal state for an event framework
		// with no consumer installed. So broadcasts call the raw SFSE proxy directly (the
		// same reinterpret CommonLibSF itself uses in its private GetProxy()), bypassing that
		// error log. Targeted dispatches (the request/response below) keep the wrapper, where
		// a false return DOES indicate a real delivery failure worth logging.
		void BroadcastQuiet(std::uint32_t a_type, void* a_data, std::uint32_t a_len)
		{
			const auto* const intfc = SFSE::GetMessagingInterface();
			if (!intfc) {
				return;
			}
			const auto* const raw = reinterpret_cast<const SFSE::Impl::SFSEMessagingInterface*>(intfc);
			raw->Dispatch(SFSE::GetPluginHandle(), a_type, a_data, a_len, nullptr);  // nullptr = broadcast
		}
	}

	Server* Server::GetSingleton()
	{
		static Server singleton;
		return &singleton;
	}

	void Server::Register()
	{
		Defeat::SetOnDefeat([](FormID a_victim, FormID a_aggressor) {
			GetSingleton()->BroadcastDefeat(a_victim, a_aggressor);
		});
		Defeat::SetOnRescue([](FormID a_victim, FormID a_aggressor) {
			GetSingleton()->BroadcastRescue(a_victim, a_aggressor);
		});
		REX::INFO("[OSFDefeat] API server registered");
	}

	void Server::OnRequest(const SFSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg || a_msg->type != OSFDefeatAPI::kRequestMessage) {
			return;
		}
		const auto* const intfc = SFSE::GetMessagingInterface();
		if (!intfc) {
			return;
		}
		OSFDefeatAPI::IVDefeat1* self = GetSingleton();
		intfc->Dispatch(OSFDefeatAPI::kResponseMessage, &self,
			static_cast<std::uint32_t>(sizeof(self)), a_msg->sender);
		REX::INFO("[OSFDefeat] served API request to {}", a_msg->sender ? a_msg->sender : "<unknown>");
	}

	void Server::BroadcastDefeat(std::uint32_t a_victim, std::uint32_t a_aggressor)
	{
		OSFDefeatAPI::DefeatEvent data{ a_victim, a_aggressor };
		BroadcastQuiet(OSFDefeatAPI::kOnDefeatMessage, &data, static_cast<std::uint32_t>(sizeof(data)));
	}

	void Server::BroadcastRescue(std::uint32_t a_victim, std::uint32_t a_aggressor)
	{
		OSFDefeatAPI::DefeatEvent data{ a_victim, a_aggressor };
		BroadcastQuiet(OSFDefeatAPI::kOnRescueMessage, &data, static_cast<std::uint32_t>(sizeof(data)));
	}

	std::uint32_t Server::Version() const noexcept
	{
		return OSFDefeatAPI::kInterfaceVersion;
	}

	bool Server::DefeatActor(RE::Actor* a_victim)
	{
		if (!a_victim) {
			return false;
		}
		const FormID      id = a_victim->GetFormID();
		const auto* const tasks = SFSE::GetTaskInterface();
		if (!tasks) {
			return false;
		}
		tasks->AddTask([id]() {
			if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
				Pipeline::OnDown(Trigger::DownEvent{ actor, nullptr });
			}
		});
		return true;
	}

	bool Server::RescueActor(RE::Actor* a_victim, bool a_undoPacify)
	{
		if (!a_victim) {
			return false;
		}
		const FormID      id = a_victim->GetFormID();
		const auto* const tasks = SFSE::GetTaskInterface();
		if (!tasks) {
			return false;
		}
		tasks->AddTask([id, a_undoPacify]() {
			if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
				Defeat::RescueActor(actor, a_undoPacify);
			}
		});
		return true;
	}

	bool Server::IsDefeated(RE::Actor* a_victim)
	{
		return Defeat::IsDefeated(a_victim);
	}

	std::uint32_t Server::RegisterOutcomeProvider(OSFDefeatAPI::OutcomeProvider a_provider, void* a_userData)
	{
		// The registry lives in the Decision layer (so SelectOutcome can consult it without a
		// Core->API dependency); the server is just the public messaging-reachable facade.
		return Decision::RegisterOutcomeProvider(a_provider, a_userData);
	}

	bool Server::UnregisterOutcomeProvider(std::uint32_t a_token)
	{
		return Decision::UnregisterOutcomeProvider(a_token);
	}
}
