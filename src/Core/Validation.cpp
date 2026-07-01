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

    Verdict ExplainPair(RE::Actor* a_victim, RE::Actor* /*a_aggressor*/)
    {
        if (!IsEnabled()) {
            return Verdict::kApiDisabled;
        }
        if (!Config::IsProcessingEnabled()) {
            return Verdict::kConfigDisabled;
        }
        if (!a_victim) {
            return Verdict::kNoVictim;
        }
        if (a_victim->IsDead()) {
            return Verdict::kVictimDead;
        }
        if (Config::ShouldDisableInScenes() && a_victim->GetCurrentScene()) {
            return Verdict::kInScene;
        }
        // GetCurrentLocation has an unmapped AddrLib ID (0) in this CommonLibSF copy and crashes if
        // called (REL/IDDB "Invalid ID: 0"), so only touch it when locations are actually excluded.
        // The default (empty) set never calls it. Populating excludedLocations needs a verified ID.
        if (Config::HasExcludedLocations() && Config::IsLocationExcluded(a_victim->GetCurrentLocation())) {
            return Verdict::kLocationExcluded;
        }
        return Verdict::kOk;
    }

    bool ValidatePair(RE::Actor* a_victim, RE::Actor* a_aggressor)
    {
        return ExplainPair(a_victim, a_aggressor) == Verdict::kOk;
    }

    std::string_view VerdictName(Verdict a_verdict)
    {
        switch (a_verdict) {
        case Verdict::kOk:
            return "ok";
        case Verdict::kApiDisabled:
            return "processing disabled via DisableProcessing API";
        case Verdict::kConfigDisabled:
            return "processingEnabled: false in validation JSON";
        case Verdict::kNoVictim:
            return "no victim";
        case Verdict::kVictimDead:
            return "victim already dead";
        case Verdict::kInScene:
            return "victim in a scene (disableInScenes)";
        case Verdict::kLocationExcluded:
            return "location excluded (excludedLocations)";
        default:
            return "unknown";
        }
    }
}
