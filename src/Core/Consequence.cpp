#include "pch.h"

#include "Core/Consequence.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace OSFDefeat::Consequence
{
	namespace
	{
		// An authored effect attached to a rule. Log-only for now (author-side testing); engine-backed
		// effects (item transfer, faction shifts) are the consumer's job via the Papyrus API.
		struct Effect
		{
			std::string message;
		};

		// A single rule: an optional encounter-class match (nullopt = wildcard) -> an optional outcome
		// (nullopt = decline / use the framework default) plus any authored effects.
		struct Rule
		{
			std::optional<std::uint32_t>     encounterClass;  // nullopt = matches any class
			std::optional<Decision::Outcome> outcome;         // nullopt = decline (framework default)
			std::vector<Effect>              effects;
		};

		// Loaded once at data load (game thread) and read-only during SelectOutcome/Fire (game
		// thread), so no lock is needed.
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
			wchar_t     buffer[MAX_PATH]{};
			const DWORD len = ::GetModuleFileNameW(thisModule, buffer, MAX_PATH);
			if (len == 0 || len >= MAX_PATH) {
				return {};
			}
			return std::filesystem::path{ buffer }.parent_path() / "OSFDefeat" / "consequences.json";
		}

		[[nodiscard]] std::optional<std::uint32_t> ParseClass(const std::string& a_class)
		{
			using EC = Decision::EncounterClass;
			if (a_class == "*" || a_class == "any") {
				return std::nullopt;  // wildcard
			}
			if (a_class == "Hostile") {
				return static_cast<std::uint32_t>(EC::Hostile);
			}
			if (a_class == "Crew") {
				return static_cast<std::uint32_t>(EC::Crew);
			}
			if (a_class == "Security") {
				return static_cast<std::uint32_t>(EC::Security);
			}
			if (a_class == "Civilian") {
				return static_cast<std::uint32_t>(EC::Civilian);
			}
			if (a_class == "Unknown") {
				return static_cast<std::uint32_t>(EC::Unknown);
			}
			return std::nullopt;  // unrecognized -> caller decides (we skip the rule)
		}

		// "hold" -> held; "release" -> recover after recoverySeconds; "default"/absent -> decline.
		[[nodiscard]] std::optional<Decision::Outcome> ParseOutcome(const nlohmann::json& a_outcome)
		{
			const std::string disposition = a_outcome.value("disposition", std::string{ "default" });
			if (disposition == "hold") {
				return Decision::Outcome{ /*hold*/ true, /*recoverySeconds*/ -1.0f };
			}
			if (disposition == "release") {
				return Decision::Outcome{ /*hold*/ false, a_outcome.value("recoverySeconds", -1.0f) };
			}
			return std::nullopt;  // "default" -> defer to the framework default
		}

		[[nodiscard]] const Rule* FindMatch(const Decision::Context& a_context)
		{
			for (const auto& rule : g_rules) {
				if (!rule.encounterClass || *rule.encounterClass == a_context.encounterClass) {
					return &rule;  // first match wins
				}
			}
			return nullptr;
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
			REX::INFO("[OSFDefeat] no consequence config ({}) - using built-in defaults", path.string());
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
							REX::WARN("[OSFDefeat] consequence rule: unknown class '{}' - skipped", className);
							continue;
						}
					}
					if (jsonRule.contains("outcome")) {
						rule.outcome = ParseOutcome(jsonRule["outcome"]);
					}
					if (jsonRule.contains("consequences") && jsonRule["consequences"].is_array()) {
						for (const auto& jsonEffect : jsonRule["consequences"]) {
							const auto effectType = jsonEffect.value("type", std::string{});
							if (effectType == "log") {
								rule.effects.push_back(Effect{ jsonEffect.value("message", std::string{}) });
							} else {
								// "event" effects need a consumer; the Papyrus API is the seam now. Skip.
								REX::WARN("[OSFDefeat] consequence rule: effect type '{}' not supported (use the Papyrus API) - skipped",
									effectType);
							}
						}
					}
					g_rules.push_back(std::move(rule));
				}
			}
			REX::INFO("[OSFDefeat] loaded {} consequence rule(s) from {}", g_rules.size(), path.string());
		} catch (const std::exception& e) {
			REX::WARN("[OSFDefeat] consequence config parse error ({}) - using built-in defaults", e.what());
			g_rules.clear();
		}
	}

	std::optional<Decision::Outcome> Evaluate(const Decision::Context& a_context)
	{
		if (const Rule* const rule = FindMatch(a_context)) {
			return rule->outcome;  // may be nullopt (decline -> framework default)
		}
		return std::nullopt;
	}

	void Fire(const Decision::Context& a_context)
	{
		const Rule* const rule = FindMatch(a_context);
		if (!rule) {
			return;
		}
		for (const auto& effect : rule->effects) {
			REX::INFO("[OSFDefeat] consequence (victim {:08X}): {}", a_context.victim, effect.message);
		}
	}
}
