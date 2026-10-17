#pragma once
#include "pch.h"
#include "Core/Capture.h"
#include "Core/Config.h"
#include "Core/Consequence.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/Eligibility.h"
#include "Core/Events.h"
#include "Core/PlayerDefeat.h"
#include "Core/Recovery.h"
#include "Core/Truce.h"
#include "Core/RuntimeMutation.h"
#include "Engine/Engine.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <source_location>

namespace Test {
    using namespace OSFDefeat;
    using Json = nlohmann::json;
    struct Case { std::string name; void (*run)(); };
    inline std::vector<Case>& Cases() { static std::vector<Case> cases; return cases; }
    struct Register { Register(const char* name, void (*fn)()) { Cases().push_back({name, fn}); } };
    inline int checks{};
    inline std::filesystem::path sandbox;
    inline void Check(bool ok, const char* expression, std::source_location location = std::source_location::current()) {
        ++checks;
        if (!ok) throw std::runtime_error(std::format("{}:{}: {}", location.file_name(), location.line(), expression));
    }
    inline bool Near(float a, float b, float tolerance = 0.03f) { return std::abs(a - b) <= tolerance; }
    inline void Write(std::string_view relative, const Json& value) {
        auto path = Config::RootPath() / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << value.dump(2);
    }
    inline void Raw(std::string_view relative, std::string_view value) {
        auto path = Config::RootPath() / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << value;
    }
    inline bool Logged(std::string_view text) {
        return std::ranges::any_of(TestEngine::logs, [&](const auto& line) { return line.find(text) != std::string::npos; });
    }
    inline void Add(RE::TESForm& form, std::string name = {}) {
        RE::TESForm::forms[form.id] = &form;
        if (!name.empty()) RE::TESForm::editors[name] = &form;
    }
    inline RE::Calendar calendar;
    inline RE::BSScript::IVirtualMachine vm;
    inline RE::GameVM gameVM;
    inline bool factionAvailable = true, factionApplyWorks = true;
    inline std::map<FormID, FormID> ownedFactions;
    inline std::set<FormID> preexistingFactions;
    inline int downCalls{}, restoreCalls{}, showCalls{}, clearCalls{}, reassertCalls{}, resetCalls{};
    inline std::function<void(RE::Actor&)> onRestore;
    inline std::string startedQuest, questContextAtStart;
    inline std::function<void()> onOriginalDamage;
    void Hit(RE::Actor& actor, float delta, RE::TESObjectREFR* aggressor = nullptr,
        RE::ACTOR_VALUE_MODIFIER modifier = RE::ACTOR_VALUE_MODIFIER::kDamage, const RE::ActorValueInfo* gauge = nullptr);
    void Reset();
}
#define TEST(name) static void name(); static Test::Register reg_##name(#name, &name); static void name()
#define CHECK(...) Test::Check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
