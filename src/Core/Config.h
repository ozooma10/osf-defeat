#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Config
{
    void Load();
    void Reload();

    [[nodiscard]] float DefaultAutoRecoveryDelaySeconds();
    [[nodiscard]] bool IsProcessingEnabled();
    [[nodiscard]] bool ShouldDisableInScenes();
    [[nodiscard]] bool IsNpcAutoDefeatEnabled();
    [[nodiscard]] bool NpcAutoDefeatIncludesRobots();
    [[nodiscard]] bool ShouldApplyCaptiveFactionOnPlayerDefeat();
    [[nodiscard]] FormID CaptiveFactionID();
    [[nodiscard]] float RecentHostileAttackerRadius();
    [[nodiscard]] bool IsExplainEnabled();
    [[nodiscard]] float WatchdogSeconds();
    [[nodiscard]] float ImminentHealthFraction();
    // Fixed game-executable-relative root shared by settings, consequences, and overrides.
    [[nodiscard]] const std::filesystem::path& RootPath();
}
