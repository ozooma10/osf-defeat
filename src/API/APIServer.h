#pragma once

#include "pch.h"

#include "API/OSFDefeatAPI.h"

namespace OSFDefeat::API
{
	// Concrete implementation of the public framework interface + the messaging
	// bridge (request/response + event broadcasts). Other SFSE plugins reach the
	// core only through here.
	class Server final : public OSFDefeatAPI::IVDefeat2
	{
	public:
		[[nodiscard]] static Server* GetSingleton();

		// Wire the core's defeat/rescue observers to event broadcasts.
		static void Register();

		// Answer an interface request by dispatching the interface back to the sender.
		void OnRequest(const SFSE::MessagingInterface::Message* a_msg);

		void BroadcastDefeat(std::uint32_t a_victim, std::uint32_t a_aggressor);
		void BroadcastRescue(std::uint32_t a_victim, std::uint32_t a_aggressor);
		void BroadcastConsequence(std::uint32_t a_victim, std::uint32_t a_aggressor, const char* a_tag);

		// --- OSFDefeatAPI::IVDefeat1 -----------------------------------------------
		[[nodiscard]] std::uint32_t Version() const noexcept override;
		bool                        DefeatActor(RE::Actor* a_victim) override;
		bool                        RescueActor(RE::Actor* a_victim, bool a_undoPacify) override;
		[[nodiscard]] bool          IsDefeated(RE::Actor* a_victim) override;

		// --- OSFDefeatAPI::IVDefeat2 -----------------------------------------------
		std::uint32_t RegisterOutcomeProvider(OSFDefeatAPI::OutcomeProvider a_provider, void* a_userData) override;
		bool          UnregisterOutcomeProvider(std::uint32_t a_token) override;
	};
}
