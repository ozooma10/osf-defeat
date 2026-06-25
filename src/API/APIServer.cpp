#include "pch.h"

#include "API/APIServer.h"

#include "Core/Defeat.h"
#include "Core/Pipeline.h"

namespace OSFDefeat::API
{
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
		const auto* const intfc = SFSE::GetMessagingInterface();
		if (!intfc) {
			return;
		}
		OSFDefeatAPI::DefeatEvent data{ a_victim, a_aggressor };
		intfc->Dispatch(OSFDefeatAPI::kOnDefeatMessage, &data,
			static_cast<std::uint32_t>(sizeof(data)), nullptr);  // nullptr = broadcast
	}

	void Server::BroadcastRescue(std::uint32_t a_victim, std::uint32_t a_aggressor)
	{
		const auto* const intfc = SFSE::GetMessagingInterface();
		if (!intfc) {
			return;
		}
		OSFDefeatAPI::DefeatEvent data{ a_victim, a_aggressor };
		intfc->Dispatch(OSFDefeatAPI::kOnRescueMessage, &data,
			static_cast<std::uint32_t>(sizeof(data)), nullptr);
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
}
