#include "pch.h"

#include "Core/Validation.h"

#include "Core/Config.h"

namespace OSFDefeat::Validation
{
    namespace
    {
        std::atomic<bool> g_enabled{ true };
    }

    void SetEnabled(bool a_enabled) { g_enabled.store(a_enabled); }
    bool IsEnabled() { return g_enabled.load(); }
    bool CanProcess() { return IsEnabled() && Config::IsProcessingEnabled(); }

    bool ValidatePair(RE::Actor* a_victim, RE::Actor* /*a_aggressor*/)
    {
        if (!CanProcess() || !a_victim || a_victim->IsDead()) {
            return false;
        }
        if (Config::ShouldDisableInScenes() && a_victim->GetCurrentScene()) {
            return false;
        }
        // GetCurrentLocation has an unmapped AddrLib ID (0) in this CommonLibSF copy and crashes if
        // called (REL/IDDB "Invalid ID: 0"), so only touch it when locations are actually excluded.
        // The default (empty) set never calls it. Populating excludedLocations needs a verified ID.
        if (Config::HasExcludedLocations() && Config::IsLocationExcluded(a_victim->GetCurrentLocation())) {
            return false;
        }
        return true;
    }
}