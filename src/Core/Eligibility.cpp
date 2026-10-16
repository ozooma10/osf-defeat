#include "pch.h"

#include "Core/Eligibility.h"

#include "Core/Config.h"

namespace OSFDefeat::Eligibility
{
    namespace
    {
        std::atomic<bool> g_enabled{ true };
    }

    void SetEnabled(bool a_enabled) { g_enabled.store(a_enabled); }
    bool IsEnabled() { return g_enabled.load(); }

    Verdict Explain(RE::Actor* a_victim, RE::Actor* /*a_aggressor*/)
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
        // excludedLocations/excludedQuests remain schema-reserved and fail closed at load time;
        // this CommonLibSF's location seam is unmapped and unsafe to call.
        return Verdict::kOk;
    }

    bool IsEligible(RE::Actor* a_victim, RE::Actor* a_aggressor)
    {
        return Explain(a_victim, a_aggressor) == Verdict::kOk;
    }

    std::string_view VerdictName(Verdict a_verdict)
    {
        switch (a_verdict) {
        case Verdict::kOk:
            return "ok";
        case Verdict::kApiDisabled:
            return "processing disabled via DisableProcessing API";
        case Verdict::kConfigDisabled:
            return "processingEnabled: false in framework settings";
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
