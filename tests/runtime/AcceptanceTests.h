#pragma once
#include <nlohmann/json.hpp>
namespace RE { class Actor; namespace BSScript { class IVirtualMachine; } }
namespace OSF::API { struct IOSFSceneAPI; }
namespace OSFDefeat::RuntimeTests::Acceptance {
    void Bind(RE::BSScript::IVirtualMachine& vm);
    void Reattached(std::uint32_t actor);
    void Saved();
    void Configure(const nlohmann::json& config);
    bool Tick(nlohmann::json& snapshot, RE::Actor* player, OSF::API::IOSFSceneAPI* scenes,
        const std::filesystem::path& directory, bool shadowsOnly = false, bool blackoutColdStart = false);
}
