#include "pch.h"

#include "Core/Recovery.h"

#include "Core/Config.h"
#include "Core/Defeat.h"
#include "Core/DefeatRegistry.h"
#include "Core/GameTime.h"
#include "Core/Presentation.h"
#include "Core/RuntimeMutation.h"
#include "REL/THook.h"

namespace OSFDefeat::Recovery
{
    namespace
    {
        constexpr REL::ID kMainLoopID{ 99488 };
        constexpr std::ptrdiff_t kMainUpdateCallOffset = 0x2D6;
        constexpr auto kTickInterval = std::chrono::milliseconds{ 500 };
        std::optional<REL::THook<void()>> g_updateHook;
        std::chrono::steady_clock::time_point g_nextTick;

        void Update()
        {
            (*g_updateHook)();
            const auto now = std::chrono::steady_clock::now();
            if (now >= g_nextTick) {
                g_nextTick = now + kTickInterval;
                Tick();
            }
        }

        [[nodiscard]] float TimeScale()
        {
            const auto* const calendar = RE::Calendar::GetSingleton();
            return calendar ? std::max(1.0f, calendar->GetTimeScale()) : 20.0f;
        }
    }

    bool Install()
    {
        // Same post-Main-update seam used by OSF Animation (native callee 99406).
        // THook chains the installed target, including another plugin's existing hook.
        g_updateHook.emplace("[OSFDefeat] Recovery::MainUpdate", kMainLoopID, kMainUpdateCallOffset, &Update);
        const auto type = g_updateHook->GetType();
        if ((type != REL::EHookType::CALL5 && type != REL::EHookType::CALL6) ||
            !g_updateHook->Init() || !g_updateHook->Enable()) {
            REX::ERROR("[OSFDefeat] recovery update hook could not be installed at 99488+0x{:X}", kMainUpdateCallOffset);
            return false;
        }
        REX::INFO("[OSFDefeat] installed direct recovery update at 99488+0x{:X} (500 ms cadence)", kMainUpdateCallOffset);
        return true;
    }

    float DeadlineForSeconds(float a_realSeconds)
    {
        return GameTime::NowDay() + ((std::max(1.0f, a_realSeconds) * TimeScale()) / 86400.0f);
    }

    bool ScheduleAutomatic(RE::Actor* a_actor, float a_delaySeconds)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_actor || !std::isfinite(a_delaySeconds) || a_delaySeconds <= 0.0f) {
            return false;
        }
        const auto activeDefeat = DefeatRegistry::Get(a_actor->GetFormID());
        if (!activeDefeat) {
            return false;
        }
        activeDefeat->autoRecoverAtDay = DeadlineForSeconds(a_delaySeconds);
        activeDefeat->leaseExpiresAtDay = 0.0f;
        activeDefeat->SetLeaseHolderId({});
        REX::INFO("[OSFDefeat] automatic recovery for {:08X} scheduled in {:.2f}s", a_actor->GetFormID(), a_delaySeconds);
        return true;
    }

    bool Hold(RE::Actor* a_actor)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_actor) {
            return false;
        }
        const auto activeDefeat = DefeatRegistry::Get(a_actor->GetFormID());
        if (!activeDefeat) {
            return false;
        }
        activeDefeat->autoRecoverAtDay = 0.0f;
        REX::INFO("[OSFDefeat] recovery for {:08X} held; awaiting explicit recovery", a_actor->GetFormID());
        return true;
    }

    bool SetTimer(RE::Actor* a_actor, float a_seconds)
    {
        if (a_seconds > 0.0f) {
            return ScheduleAutomatic(a_actor, a_seconds);
        }
        return std::isfinite(a_seconds) && Hold(a_actor);
    }

    float SecondsRemaining(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return 0.0f;
        }
        const auto activeDefeat = DefeatRegistry::Get(a_actor->GetFormID());
        if (!activeDefeat) {
            return 0.0f;
        }
        const float autoRecoverAtDay = activeDefeat->autoRecoverAtDay.load();
        if (autoRecoverAtDay <= 0.0f) {
            return -1.0f;
        }
        return std::max(0.0f, (autoRecoverAtDay - GameTime::NowDay()) * 86400.0f / TimeScale());
    }

    bool AcquireLease(RE::Actor* a_actor, std::string_view a_holderId, float a_timeoutSeconds)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_actor || a_holderId.empty() || !std::isfinite(a_timeoutSeconds)) {
            return false;
        }
        const auto activeDefeat = DefeatRegistry::Get(a_actor->GetFormID());
        if (!activeDefeat) {
            return false;
        }
        const auto currentHolderId = activeDefeat->CopyLeaseHolderId();
        if (!currentHolderId.empty() && currentHolderId != a_holderId) {
            REX::WARN("[OSFDefeat] defeat lease on {:08X} refused: held by '{}' (requested by '{}')",
                a_actor->GetFormID(), currentHolderId, a_holderId);
            return false;
        }

        const float timeoutSeconds = a_timeoutSeconds > 0.0f ? a_timeoutSeconds : Config::WatchdogSeconds();
        activeDefeat->autoRecoverAtDay = 0.0f;
        activeDefeat->SetLeaseHolderId(std::string{ a_holderId });
        activeDefeat->leaseExpiresAtDay = timeoutSeconds > 0.0f ? DeadlineForSeconds(timeoutSeconds) : 0.0f;
        if (timeoutSeconds > 0.0f) {
            REX::INFO("[OSFDefeat] '{}' acquired defeat lease on {:08X} for up to {:.0f}s",
                a_holderId, a_actor->GetFormID(), timeoutSeconds);
        } else {
            REX::INFO("[OSFDefeat] '{}' acquired defeat lease on {:08X} with no deadline (watchdog disabled)",
                a_holderId, a_actor->GetFormID());
        }
        return true;
    }

    bool ReleaseLease(RE::Actor* a_actor, std::string_view a_holderId)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        if (!a_actor) {
            return false;
        }
        const auto activeDefeat = DefeatRegistry::Get(a_actor->GetFormID());
        if (!activeDefeat) {
            return false;
        }
        const auto currentHolderId = activeDefeat->CopyLeaseHolderId();
        if (currentHolderId.empty()) {
            return false;
        }
        if (currentHolderId != a_holderId) {
            REX::WARN("[OSFDefeat] defeat lease release on {:08X} refused: held by '{}', not '{}'",
                a_actor->GetFormID(), currentHolderId, a_holderId);
            return false;
        }

        activeDefeat->SetLeaseHolderId({});
        activeDefeat->leaseExpiresAtDay = 0.0f;
        activeDefeat->autoRecoverAtDay = DeadlineForSeconds(Config::DefaultAutoRecoveryDelaySeconds());
        REX::INFO("[OSFDefeat] '{}' released defeat lease on {:08X}; default recovery resumes",
            a_holderId, a_actor->GetFormID());
        return true;
    }

    std::string GetLeaseHolder(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return {};
        }
        const auto activeDefeat = DefeatRegistry::Get(a_actor->GetFormID());
        return activeDefeat ? activeDefeat->CopyLeaseHolderId() : std::string{};
    }

    void Tick()
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        const float now = GameTime::NowDay();
        const float watchdogSeconds = Config::WatchdogSeconds();
        for (const auto actorId : DefeatRegistry::Snapshot()) {
            const auto activeDefeat = DefeatRegistry::Get(actorId);
            if (!activeDefeat) {
                continue;
            }

            if (activeDefeat->autoRecoverAtDay > 0.0f) {
                if (now >= activeDefeat->autoRecoverAtDay) {
                    if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                        Defeat::RecoverActor(actor, true);
                    }
                }
                continue;
            }

            if (activeDefeat->leaseExpiresAtDay <= 0.0f) {
                if (actorId == 0x14 && watchdogSeconds > 0.0f) {
                    activeDefeat->leaseExpiresAtDay = DeadlineForSeconds(watchdogSeconds);
                    if (activeDefeat->CopyLeaseHolderId().empty()) {
                        activeDefeat->SetLeaseHolderId("(unleased recovery hold)");
                    }
                }
                continue;
            }
            if (now >= activeDefeat->leaseExpiresAtDay) {
                const auto holderId = activeDefeat->CopyLeaseHolderId();
                REX::WARN("[OSFDefeat] watchdog: held down-state of {:08X} expired (lease holder '{}' never extended or recovered) - force-recovering",
                    actorId, holderId.empty() ? "(unleased hold)" : holderId);
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(actorId)) {
                    Defeat::RecoverActor(actor, true);
                } else {
                    DefeatRegistry::Remove(actorId);
                }
            }
        }

        auto* const player = static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton());
        if (player && Defeat::IsDefeated(player)) {
            Presentation::ReassertPlayerDown();
        }
    }
}
