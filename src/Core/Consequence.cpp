#include "pch.h"

#include "Core/Consequence.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace OSFDefeat::Consequence
{
	namespace
	{
		// A single rule: an optional encounter-class match (nullopt = wildcard) -> an outcome.
		struct Rule
		{
			std::optional<std::uint32_t> encounterClass;  // nullopt = matches any class
			OSFDefeatAPI::DefeatOutcome  outcome;
		};

		// Loaded once at data load (game thread) and read-only during SelectOutcome (game thread),
		// so no lock is needed.
		std::vector<Rule> g_rules;

		// Resolve OSFDefeat/consequences.json next to THIS DLL (robust under MO2's VFS, which maps
		// both the DLL and the sibling config into the virtual Data tree).
		[[nodiscard]] std::filesystem::path ConfigPath()
		{
			HMODULE thisModule = nullptr;
			if (!::GetModuleHandleExW(
					GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					reinterpret_cast<LPCWSTR>(&ConfigPath), &thisModule)) {
				return {};
			}
			wchar_t      buffer[MAX_PATH]{};
			const DWORD  len = ::GetModuleFileNameW(thisModule, buffer, MAX_PATH);
			if (len == 0 || len >= MAX_PATH) {
				return {};
			}
			return std::filesystem::path{ buffer }.parent_path() / "OSFDefeat" / "consequences.json";
		}

		[[nodiscard]] std::optional<std::uint32_t> ParseClass(const std::string& a_class)
		{
			using EC = OSFDefeatAPI::EncounterClass;
			if (a_class == "*" || a_class == "any") {
				return std::nullopt;  // wildcard
			}
			if (a_class == "Hostile") {
				return static_cast<std::uint32_t>(EC::kHostile);
			}
			if (a_class == "Crew") {
				return static_cast<std::uint32_t>(EC::kCrew);
			}
			if (a_class == "Security") {
				return static_cast<std::uint32_t>(EC::kSecurity);
			}
			if (a_class == "Civilian") {
				return static_cast<std::uint32_t>(EC::kCivilian);
			}
			if (a_class == "Unknown") {
				return static_cast<std::uint32_t>(EC::kUnknown);
			}
			return std::nullopt;  // unrecognized -> caller decides (we skip the rule)
		}

		[[nodiscard]] OSFDefeatAPI::DefeatOutcome ParseOutcome(const nlohmann::json& a_outcome)
		{
			using Disposition = OSFDefeatAPI::DefeatOutcome::Disposition;
			OSFDefeatAPI::DefeatOutcome out{};
			const std::string disposition = a_outcome.value("disposition", std::string{ "default" });
			if (disposition == "hold") {
				out.disposition = Disposition::kHold;
			} else if (disposition == "release") {
				out.disposition = Disposition::kRelease;
			} else {
				out.disposition = Disposition::kDefault;
			}
			out.recoverySeconds = a_outcome.value("recoverySeconds", -1.0f);
			return out;
		}
	}

	void Load()
	{
		g_rules.clear();

		const auto path = ConfigPath();
		if (path.empty()) {
			REX::WARN("[OSFDefeat] consequence config path unresolved - using built-in defaults");
			return;
		}

		std::ifstream file{ path };
		if (!file) {
			REX::INFO("[OSFDefeat] no consequence config ({}) - using built-in defaults",
				path.string());
			return;
		}

		try {
			// allow_exceptions = true (caught below), ignore_comments = true (author-friendly).
			const nlohmann::json json = nlohmann::json::parse(file, nullptr, true, true);
			if (json.contains("rules") && json["rules"].is_array()) {
				for (const auto& jsonRule : json["rules"]) {
					Rule rule;
					if (jsonRule.contains("class")) {
						const auto className = jsonRule["class"].get<std::string>();
						rule.encounterClass = ParseClass(className);
						if (!rule.encounterClass && className != "*" && className != "any") {
							REX::WARN("[OSFDefeat] consequence rule: unknown class '{}' - skipped",
								className);
							continue;
						}
					}
					if (jsonRule.contains("outcome")) {
						rule.outcome = ParseOutcome(jsonRule["outcome"]);
					}
					g_rules.push_back(std::move(rule));
				}
			}
			REX::INFO("[OSFDefeat] loaded {} consequence rule(s) from {}", g_rules.size(),
				path.string());
		} catch (const std::exception& e) {
			REX::WARN("[OSFDefeat] consequence config parse error ({}) - using built-in defaults",
				e.what());
			g_rules.clear();
		}
	}

	std::optional<OSFDefeatAPI::DefeatOutcome> Evaluate(const OSFDefeatAPI::DefeatContext& a_context)
	{
		for (const auto& rule : g_rules) {
			if (!rule.encounterClass || *rule.encounterClass == a_context.encounterClass) {
				return rule.outcome;  // first match wins
			}
		}
		return std::nullopt;
	}
}
