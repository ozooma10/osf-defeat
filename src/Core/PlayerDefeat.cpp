#include "pch.h"

#include "Core/PlayerDefeat.h"

#include "Core/Config.h"
#include "Core/Consequence.h"
#include "Core/Decision.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/Events.h"
#include "Core/DefeatRegistry.h"
#include "Core/Presentation.h"
#include "Core/QuestHandoff.h"
#include "Core/Validation.h"
#include "Engine/Engine.h"

#include "REL/ASM.h"
#include "REL/Utility.h"

namespace OSFDefeat::PlayerDefeat
{
    namespace
    {
        using ModActorValue_t = void(RE::ActorValueOwner*, RE::ACTOR_VALUE_MODIFIER, const RE::ActorValueInfo&, float, RE::TESObjectREFR*);

        constexpr REL::ID kModActorValueID{ 101587 };
        constexpr std::array<std::uint8_t, 9> kExpectedPrologue{ 0x48, 0x89, 0x5C, 0x24, 0x08, 0x89, 0x54, 0x24, 0x10 };
        constexpr std::size_t kStolenLength = kExpectedPrologue.size();
        constexpr float kLethalHealthFloor = 5.0f;

        ModActorValue_t* g_original = nullptr;
        std::atomic_bool g_hooked{ false };
        std::atomic_bool g_pendingDefeat{ false };
        std::atomic_bool g_workerStarted{ false };

        // --- Participant recording -----------------------------------------------------------------
        // Who damaged the player recently, fed passively by the hook below. This is the participant
        // source for the defeat-time snapshot: live ProcessLists actor arrays must NOT be iterated from
        // an SFSE task (proven crash: races the sim thread's realloc → AV in GetSmartPointer), so we
        // only store PODs here (FormID + timestamp, worker thread) and resolve via LookupByID on the
        // game thread later. "Damaged the player in the last window" is also the better cast list —
        // actual combatants, not bystanders.
        constexpr auto kParticipantWindow = std::chrono::seconds{ 30 };
        constexpr std::size_t kParticipantCap = 64;
        std::map<FormID, std::chrono::steady_clock::time_point> g_recentAttackers;
        std::mutex g_recentAttackersLock;

        void RecordAttacker(RE::TESObjectREFR* a_ref)
        {
            if (!a_ref) {
                return;
            }
            const FormID id = a_ref->GetFormID();
            const auto now = std::chrono::steady_clock::now();
            std::unique_lock lock{ g_recentAttackersLock };
            std::erase_if(g_recentAttackers, [&](const auto& entry) { return now - entry.second > kParticipantWindow; });
            if (g_recentAttackers.size() < kParticipantCap || g_recentAttackers.contains(id)) {
                g_recentAttackers[id] = now;
            }
        }

        [[nodiscard]] std::vector<FormID> RecentAttackerIDs()
        {
            const auto now = std::chrono::steady_clock::now();
            std::vector<FormID> out;
            std::unique_lock lock{ g_recentAttackersLock };
            for (const auto& [id, when] : g_recentAttackers) {
                if (now - when <= kParticipantWindow) {
                    out.push_back(id);
                }
            }
            return out;
        }

        // Defeat-time participant snapshot: the aggressor plus every recent attacker that is still a
        // live hostile within a_radius (<= 0 = no radius filter). Game thread (LookupByID + engine
        // reads). Recorded attackers that resolve to non-actors (traps, hazards) drop out here.
        [[nodiscard]] std::vector<RE::Actor*> CollectParticipants(RE::Actor* a_player, RE::Actor* a_aggressor, float a_radius)
        {
            std::vector<RE::Actor*> out;
            if (!a_player) {
                return out;
            }

            const auto origin = a_player->GetPosition();
            const float radiusSq = a_radius > 0.0f ? a_radius * a_radius : 0.0f;
            const auto consider = [&](RE::Actor* a_actor, bool a_requireHostile) {
                if (!a_actor || a_actor == a_player || a_actor->IsDead()) {
                    return;
                }
                if (std::find(out.begin(), out.end(), a_actor) != out.end()) {
                    return;
                }
                if (a_requireHostile && !a_actor->IsHostileToActor(a_player)) {
                    return;
                }
                if (radiusSq > 0.0f && (a_actor->GetPosition() - origin).SqrLength() > radiusSq) {
                    return;
                }
                out.push_back(a_actor);
            };

            // The aggressor is always cast (no hostility re-check: the captive-faction pacify or the
            // target-list drop may already have cleared formal hostility by the time content asks).
            consider(a_aggressor, false);
            for (const auto id : RecentAttackerIDs()) {
                consider(RE::TESForm::LookupByID<RE::Actor>(id), true);
            }
            return out;
        }

        // --- Resist windows (struggle primitive) ---------------------------------------------------
        // Per-actor no-defeat grace: while active, would-be-lethal hits pass through untouched.
        // POD map + mutex so the damage worker thread can check it inline.
        std::map<FormID, std::chrono::steady_clock::time_point> g_resistWindows;
        std::mutex g_resistLock;

        [[nodiscard]] bool ResistWindowActive(FormID a_id)
        {
            const auto now = std::chrono::steady_clock::now();
            std::unique_lock lock{ g_resistLock };
            const auto it = g_resistWindows.find(a_id);
            if (it == g_resistWindows.end()) {
                return false;
            }
            if (now >= it->second) {
                g_resistWindows.erase(it);
                return false;
            }
            return true;
        }

        // --- Defeat-imminent (struggle primitive) --------------------------------------------------
        // One-shot signal when a Health hit takes the player below imminentHealthFraction x max.
        // Re-arms when a later hit lands while the player is back above the threshold (healing), and
        // on defeat itself (so the next fight can fire again). Worker thread sets, game thread
        // consumes via a queued broadcast.
        std::atomic_bool g_imminentSignaled{ false };

        void QueueImminentBroadcast()
        {
            if (const auto* const tasks = SFSE::GetTaskInterface()) {
                tasks->AddTask([]() {
                    auto* const player = static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton());
                    if (player && !Defeat::IsDefeated(player)) {
                        Events::BroadcastPlayerDefeatImminent();
                    }
                });
            }
        }

        void MaybeSignalImminent(RE::ActorValueOwner* a_owner, const RE::ActorValueInfo& a_info, float a_current, float a_delta)
        {
            const float fraction = Config::ImminentHealthFraction();
            if (fraction <= 0.0f || &a_info != Engine::HealthAV()) {
                return;
            }
            const float maxValue = a_owner->GetPermanentActorValue(a_info);
            if (maxValue <= 0.0f) {
                return;
            }
            if (a_current + a_delta <= maxValue * fraction) {
                if (!g_imminentSignaled.exchange(true)) {
                    QueueImminentBroadcast();
                }
            } else {
                g_imminentSignaled.store(false);  // back above the threshold: re-arm
            }
        }

        [[nodiscard]] float NowDay()
        {
            const auto* const cal = RE::Calendar::GetSingleton();
            return cal ? cal->GetDaysPassedExact() : 0.0f;
        }

        [[nodiscard]] float TimeScale()
        {
            const auto* const cal = RE::Calendar::GetSingleton();
            return cal ? std::max(1.0f, cal->GetTimeScale()) : 20.0f;
        }

        [[nodiscard]] RE::Actor* PlayerActor()
        {
            return static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton());
        }

        [[nodiscard]] bool IsPlayerAVOwner(RE::ActorValueOwner* a_owner)
        {
            auto* const player = PlayerActor();
            return player && a_owner == Engine::AVOwnerOf(player);
        }

        void QueueBegin(RE::TESObjectREFR* a_ref, DefeatCause a_cause)
        {
            bool expected = false;
            if (!g_pendingDefeat.compare_exchange_strong(expected, true)) {
                return;
            }

            const FormID aggressorId = a_ref ? a_ref->GetFormID() : 0;
            if (const auto* const tasks = SFSE::GetTaskInterface()) {
                tasks->AddTask([aggressorId, a_cause]() {
                    auto* aggressor = aggressorId ? RE::TESForm::LookupByID<RE::Actor>(aggressorId) : nullptr;
                    // The slot-7 wrapper (Papyrus/console DamageValue) passes a_ref=null, so there is
                    // no direct dealer. Fall back to who the player was fighting (currentCombatTarget),
                    // resolved here on the game thread. NOT myKiller — that is only written once a kill
                    // commits, which the clamp prevents (disproven in-game; see HookTrigger.md).
                    if (!aggressor) {
                        aggressor = Engine::ResolveCombatTarget(PlayerActor());
                    }
                    BeginPlayerDefeat(aggressor, a_cause);
                    g_pendingDefeat.store(false);
                });
            } else {
                g_pendingDefeat.store(false);
            }
        }

        // Both AVs reach a terminal state through this same ModActorValue primitive: Health -> death,
        // Electromagnetic Health -> the vanilla EM shock/incapacitation (OSF RE em-stun-damage-routing,
        // 2026-06-25). We clamp both identically so neither terminal path fires.
        [[nodiscard]] bool IsLethalGauge(const RE::ActorValueInfo& a_info)
        {
            return &a_info == Engine::HealthAV() || &a_info == Engine::EMHealthAV();
        }

        // Health keeps its flat floor (unchanged behavior). The EM Health gauge can be far smaller, so
        // cap the floor below its max — em-stun-damage-routing flagged a flat 5.0 as too high for a tiny
        // EM gauge. Falls back to the flat floor if the gauge max is unavailable.
        [[nodiscard]] float LethalFloorFor(RE::ActorValueOwner* a_owner, const RE::ActorValueInfo& a_info)
        {
            if (&a_info == Engine::HealthAV()) {
                return kLethalHealthFloor;
            }
            const float maxValue = a_owner ? a_owner->GetPermanentActorValue(a_info) : 0.0f;
            if (maxValue <= 0.0f) {
                return kLethalHealthFloor;
            }
            return std::min(kLethalHealthFloor, std::max(1.0f, maxValue * 0.05f));
        }

        // --- NPC auto-defeat (opt-in) ------------------------------------------------------------
        // Symmetric with the player clamp, just resolving the victim from the hooked AV owner and gating
        // on hostility/flesh. Classification (IsHostileToActor/HasKeyword) runs on the damage worker
        // thread — confirmed safe in the OSF RE sandbox (2026-06-27: 3821 concurrent worker-thread calls
        // across 22 threads, zero faults; game-thread cross-check consistent).

        // Reverse the AVOwnerOf cast: ActorValueOwner is a fixed-offset base of TESObjectREFR, so recover
        // the actor by subtracting that (constant) subobject offset, then RTTI-filter to an Actor.
        [[nodiscard]] RE::Actor* ActorFromAVOwner(RE::ActorValueOwner* a_owner)
        {
            if (!a_owner) {
                return nullptr;
            }
            static const std::ptrdiff_t avOffset = []() {
                auto* const fake = reinterpret_cast<RE::TESObjectREFR*>(std::uintptr_t{ 0x1000 });
                return reinterpret_cast<std::uint8_t*>(static_cast<RE::ActorValueOwner*>(fake)) -
                       reinterpret_cast<std::uint8_t*>(fake);
            }();
            auto* const refr = reinterpret_cast<RE::TESObjectREFR*>(reinterpret_cast<std::uint8_t*>(a_owner) - avOffset);
            return refr->As<RE::Actor>();
        }

        void QueueNpcDefeat(FormID a_victimId, FormID a_aggressorId, DefeatCause a_cause)
        {
            const auto* const tasks = SFSE::GetTaskInterface();
            if (!tasks) {
                return;
            }
            tasks->AddTask([a_victimId, a_aggressorId, a_cause]() {
                auto* const victim = a_victimId ? RE::TESForm::LookupByID<RE::Actor>(a_victimId) : nullptr;
                if (!victim || Defeat::IsDefeated(victim)) {
                    return;
                }
                auto* aggressor = a_aggressorId ? RE::TESForm::LookupByID<RE::Actor>(a_aggressorId) : nullptr;
                if (!aggressor) {
                    aggressor = Engine::ResolveCombatTarget(victim);
                }
                if (!Validation::CanProcess() || !Validation::ValidatePair(victim, aggressor)) {
                    return;
                }
                if (Defeat::DefeatActor(victim, aggressor, a_cause)) {
                    if (auto data = DefeatRegistry::Get(a_victimId)) {
                        data->recoverAtDay = RecoveryDeadlineForSeconds(Config::Get().defaultRecoverySeconds);
                    }
                }
            });
        }

        // Worker-thread classification: auto-defeat hostile NPCs (flesh, or robots when configured).
        [[nodiscard]] bool ShouldAutoDefeatNpc(RE::Actor* a_victim)
        {
            auto* const player = PlayerActor();
            if (!player || !Decision::IsHostile(a_victim, player)) {
                return false;
            }
            return !Decision::IsNonFlesh(a_victim) || Config::NpcAutoDefeatIncludesRobots();
        }

        // Legible death boundary: every deliberate player passthrough is logged with its reason, so
        // "why didn't it save me" is answerable from the log. Debounced (repeat hits while dying all
        // share one reason) and INFO-level — a player death is rare and always worth a line.
        void LogPlayerPassthrough(std::string_view a_reason)
        {
            static std::atomic<std::int64_t> lastLogMs{ 0 };
            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            auto last = lastLogMs.load();
            if (nowMs - last < 1000 || !lastLogMs.compare_exchange_strong(last, nowMs)) {
                return;
            }
            REX::INFO("[OSFDefeat] lethal player hit passed through: {}", a_reason);
        }

        void ModActorValueDetour(
            RE::ActorValueOwner* a_owner,
            RE::ACTOR_VALUE_MODIFIER a_modifier,
            const RE::ActorValueInfo& a_info,
            float a_delta,
            RE::TESObjectREFR* a_ref)
        {
            if (g_original && a_modifier == RE::ACTOR_VALUE_MODIFIER::kDamage && a_delta < 0.0f && IsLethalGauge(a_info)) {
                const auto cause = &a_info == Engine::EMHealthAV() ? DefeatCause::kEM : DefeatCause::kHealth;
                if (IsPlayerAVOwner(a_owner)) {
                    RecordAttacker(a_ref);  // participant snapshot source (POD-only on this thread)
                    auto* const player = PlayerActor();
                    const float current = a_owner->GetActorValue(a_info);
                    const bool  lethal = current + a_delta <= 0.0f;
                    auto* const aggressor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
                    if (!Defeat::IsDefeated(player)) {
                        if (!lethal) {
                            MaybeSignalImminent(a_owner, a_info, current, a_delta);
                        } else {
                            const auto verdict = Validation::ExplainPair(player, aggressor);
                            if (verdict != Validation::Verdict::kOk) {
                                LogPlayerPassthrough(Validation::VerdictName(verdict));
                            } else if (ResistWindowActive(player->GetFormID())) {
                                LogPlayerPassthrough("resist window active (ResistDefeat) - vanilla lethality");
                            } else {
                                a_delta = std::max(a_delta, LethalFloorFor(a_owner, a_info) - current);
                                QueueBegin(a_ref, cause);
                            }
                        }
                    }
                } else if (Config::IsNpcAutoDefeatEnabled()) {
                    auto* const victim = ActorFromAVOwner(a_owner);
                    if (victim && !victim->IsPlayerRef()) {
                        const float current = a_owner->GetActorValue(a_info);
                        const bool  lethal = current + a_delta <= 0.0f;
                        auto* const aggressor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
                        if (lethal && !Defeat::IsDefeated(victim)) {
                            const auto verdict = Validation::ExplainPair(victim, aggressor);
                            if (verdict != Validation::Verdict::kOk) {
                                if (Config::IsExplainEnabled()) {
                                    REX::INFO("[OSFDefeat] lethal NPC hit on {:08X} passed through: {}",
                                        victim->GetFormID(), Validation::VerdictName(verdict));
                                }
                            } else if (!ShouldAutoDefeatNpc(victim)) {
                                if (Config::IsExplainEnabled()) {
                                    REX::INFO("[OSFDefeat] lethal NPC hit on {:08X} passed through: not a hostile flesh target (or robots excluded)",
                                        victim->GetFormID());
                                }
                            } else if (ResistWindowActive(victim->GetFormID())) {
                                if (Config::IsExplainEnabled()) {
                                    REX::INFO("[OSFDefeat] lethal NPC hit on {:08X} passed through: resist window active", victim->GetFormID());
                                }
                            } else {
                                a_delta = std::max(a_delta, LethalFloorFor(a_owner, a_info) - current);
                                QueueNpcDefeat(victim->GetFormID(), a_ref ? a_ref->GetFormID() : 0, cause);
                            }
                        }
                    }
                }
            }

            if (g_original) {
                g_original(a_owner, a_modifier, a_info, a_delta, a_ref);
            }
        }

        [[nodiscard]] bool InstallHook()
        {
            const auto target = kModActorValueID.address();
            const auto* const bytes = reinterpret_cast<const std::uint8_t*>(target);
            if (!std::equal(kExpectedPrologue.begin(), kExpectedPrologue.end(), bytes)) {
                REX::ERROR("[OSFDefeat] ModActorValue prologue mismatch; player lethal hook disabled");
                return false;
            }

            auto& trampoline = REL::GetTrampoline();
            auto* const gateway = static_cast<std::uint8_t*>(trampoline.allocate(kStolenLength + sizeof(REL::ASM::JMP14)));
            std::memcpy(gateway, bytes, kStolenLength);
            new (gateway + kStolenLength) REL::ASM::JMP14{ target + kStolenLength };
            g_original = reinterpret_cast<ModActorValue_t*>(gateway);

            const auto branch = trampoline.allocate_branch5(reinterpret_cast<std::uintptr_t>(&ModActorValueDetour));
            const REL::ASM::JMP5 jmp{ target, branch };
            if (!REL::WriteSafeData(target, jmp)) {
                REX::ERROR("[OSFDefeat] failed to patch ModActorValue hook");
                return false;
            }
            if constexpr (kStolenLength > sizeof(jmp)) {
                (void)REL::WriteSafeFill(target + sizeof(jmp), 0x90, kStolenLength - sizeof(jmp));
            }

            REX::INFO("[OSFDefeat] installed player lethal damage hook at {:X}", target);
            return true;
        }

        void StartRecoveryWorker()
        {
            bool expected = false;
            if (!g_workerStarted.compare_exchange_strong(expected, true)) {
                return;
            }

            std::thread([]() {
                for (;;) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    if (const auto* const tasks = SFSE::GetTaskInterface()) {
                        tasks->AddTask([]() { TickRecovery(); });
                    }
                }
            }).detach();
        }
    }

    float RecoveryDeadlineForSeconds(float a_realSeconds)
    {
        return NowDay() + ((std::max(1.0f, a_realSeconds) * TimeScale()) / 86400.0f);
    }

    bool SetRecoveryTimer(RE::Actor* a_actor, float a_seconds)
    {
        if (!a_actor) {
            return false;
        }
        const auto data = DefeatRegistry::Get(a_actor->GetFormID());
        if (!data) {
            return false;
        }
        if (a_seconds > 0.0f) {
            // A positive timer puts the entry back on framework auto-recovery; any lease is over.
            data->recoverAtDay = RecoveryDeadlineForSeconds(a_seconds);
            data->leaseDeadlineDay = 0.0f;
            data->SetLeaseOwner({});
            REX::INFO("[OSFDefeat] recovery timer for {:08X} set to {:.2f}s", a_actor->GetFormID(), a_seconds);
        } else {
            // Hold. An existing lease keeps its own deadline; an unleased player hold gets the
            // watchdog deadline armed by the recovery tick.
            data->recoverAtDay = 0.0f;
            REX::INFO("[OSFDefeat] recovery for {:08X} held; awaiting explicit rescue", a_actor->GetFormID());
        }
        return true;
    }

    bool ClaimDefeat(RE::Actor* a_actor, std::string_view a_owner, float a_maxHoldSeconds)
    {
        if (!a_actor || a_owner.empty()) {
            return false;
        }
        const auto data = DefeatRegistry::Get(a_actor->GetFormID());
        if (!data) {
            return false;
        }
        const auto currentOwner = data->CopyLeaseOwner();
        if (!currentOwner.empty() && currentOwner != a_owner) {
            REX::WARN("[OSFDefeat] claim on {:08X} refused: already leased by '{}' (requested by '{}')",
                a_actor->GetFormID(), currentOwner, a_owner);
            return false;
        }

        const float holdSeconds = a_maxHoldSeconds > 0.0f ? a_maxHoldSeconds : Config::WatchdogSeconds();
        data->recoverAtDay = 0.0f;  // held: the owner ends it via RescueActor / ReleaseClaim / SetRecoveryTimer
        data->SetLeaseOwner(std::string{ a_owner });
        data->leaseDeadlineDay = holdSeconds > 0.0f ? RecoveryDeadlineForSeconds(holdSeconds) : 0.0f;
        if (holdSeconds > 0.0f) {
            REX::INFO("[OSFDefeat] '{}' claimed down-state of {:08X} for up to {:.0f}s", a_owner, a_actor->GetFormID(), holdSeconds);
        } else {
            REX::INFO("[OSFDefeat] '{}' claimed down-state of {:08X} with no deadline (watchdog disabled)", a_owner, a_actor->GetFormID());
        }
        return true;
    }

    bool ReleaseClaim(RE::Actor* a_actor, std::string_view a_owner)
    {
        if (!a_actor) {
            return false;
        }
        const auto data = DefeatRegistry::Get(a_actor->GetFormID());
        if (!data) {
            return false;
        }
        const auto currentOwner = data->CopyLeaseOwner();
        if (currentOwner.empty()) {
            return false;
        }
        if (currentOwner != a_owner) {
            REX::WARN("[OSFDefeat] release of {:08X} refused: leased by '{}', not '{}'",
                a_actor->GetFormID(), currentOwner, a_owner);
            return false;
        }

        data->SetLeaseOwner({});
        data->leaseDeadlineDay = 0.0f;
        data->recoverAtDay = RecoveryDeadlineForSeconds(Config::Get().defaultRecoverySeconds);
        REX::INFO("[OSFDefeat] '{}' released its claim on {:08X}; default recovery resumes", a_owner, a_actor->GetFormID());
        return true;
    }

    std::string GetClaimOwner(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return {};
        }
        const auto data = DefeatRegistry::Get(a_actor->GetFormID());
        return data ? data->CopyLeaseOwner() : std::string{};
    }

    bool SetResistWindow(RE::Actor* a_actor, float a_seconds)
    {
        if (!a_actor) {
            return false;
        }
        const FormID id = a_actor->GetFormID();
        if (a_seconds <= 0.0f) {
            std::unique_lock lock{ g_resistLock };
            g_resistWindows.erase(id);
            return true;
        }
        if (Defeat::IsDefeated(a_actor)) {
            return false;  // a resist window is not a rescue
        }
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds{ static_cast<std::int64_t>(a_seconds * 1000.0f) };
        {
            std::unique_lock lock{ g_resistLock };
            g_resistWindows[id] = deadline;
        }
        REX::INFO("[OSFDefeat] resist window on {:08X} for {:.1f}s (vanilla lethality while active)", id, a_seconds);
        return true;
    }

    bool IsResistWindowActive(RE::Actor* a_actor)
    {
        return a_actor && ResistWindowActive(a_actor->GetFormID());
    }

    float GetRecoverySecondsRemaining(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return 0.0f;
        }
        const auto data = DefeatRegistry::Get(a_actor->GetFormID());
        if (!data) {
            return 0.0f;
        }
        if (data->recoverAtDay <= 0.0f) {
            return -1.0f;
        }
        return std::max(0.0f, (data->recoverAtDay - NowDay()) * 86400.0f / TimeScale());
    }

    void BeginPlayerDefeat(RE::Actor* a_aggressor, DefeatCause a_cause)
    {
        auto* const player = PlayerActor();
        if (!Validation::CanProcess() || !Validation::ValidatePair(player, a_aggressor) || Defeat::IsDefeated(player)) {
            return;
        }

        const auto outcome = Consequence::SelectPlayerOutcome(player, a_aggressor, a_cause);
        // Snapshot participants BEFORE the down state applies: DefeatActor drops the player from
        // combatant target lists (and may captive-pacify), which can clear the hostility the
        // participant filter keys on.
        const auto participants = CollectParticipants(player, a_aggressor, Config::ParticipantScanRadius());
        if (!Defeat::DefeatActor(player, a_aggressor, a_cause)) {
            return;
        }
        Consequence::MarkFired(outcome, player);  // stamp cooldown / oncePerLocation arbitration state
        g_imminentSignaled.store(false);          // re-arm the defeat-imminent signal for the next fight

        if (auto data = DefeatRegistry::Get(player->GetFormID())) {
            if (outcome.holdRecovery) {
                // Held for content: the consequence owns the down-state and must end it via
                // RescueActor (or SetRecoveryTimer / ReleaseClaim). The lease is armed on its
                // behalf so a consequence that errors out gets watchdog-rescued, not stranded.
                const float hold = outcome.maxHoldSeconds > 0.0f ? outcome.maxHoldSeconds : Config::WatchdogSeconds();
                data->recoverAtDay = 0.0f;
                data->SetLeaseOwner(outcome.name);
                data->leaseDeadlineDay = hold > 0.0f ? RecoveryDeadlineForSeconds(hold) : 0.0f;
            } else {
                data->recoverAtDay = RecoveryDeadlineForSeconds(outcome.recoverySeconds);
            }
        }

        if (outcome.holdRecovery) {
            REX::INFO("[OSFDefeat] player outcome '{}' selected; recovery held for content", outcome.name);
        } else {
            REX::INFO("[OSFDefeat] player outcome '{}' selected; recovery in {:.2f}s", outcome.name, outcome.recoverySeconds);
        }

        // Stage the handoff payload (read back through the GetLastPlayer* Papyrus
        // getters) — including the participant snapshot (recent attackers, Acheron
        // member-list parity) — then start the consequence quest, then notify content
        // consumers such as OSF Surrender. Context-before-quest so a quest's
        // OnQuestInit can already read the payload. We are on the game thread here,
        // so the VM dispatches are safe.
        DefeatContext::SetPlayerContext(player, a_aggressor, outcome, participants, a_cause);
        if (outcome.quest && outcome.startQuest) {
            QuestHandoff::TryStartQuestToken(*outcome.quest);
        }
        Events::BroadcastPlayerConsequenceStarted();
    }

    void TickRecovery()
    {
        // Auto-recover every defeated actor whose deadline has passed. Held entries (recoverAtDay <= 0)
        // are governed by the watchdog instead: the player's held entries always carry a lease deadline
        // (auto-armed here if a consumer held without claiming), NPC held entries only when content
        // claimed them — a held NPC captive is a legitimate long-term state. A lapsed deadline means the
        // owning consumer stopped, errored out, or forgot to extend: force-rescue so the player is never
        // stranded, and log who defaulted. Snapshot is a copy of IDs, so RescueActor mutating the
        // registry mid-loop is safe.
        const float now = NowDay();
        const float watchdog = Config::WatchdogSeconds();
        for (const auto id : DefeatRegistry::Snapshot()) {
            const auto data = DefeatRegistry::Get(id);
            if (!data) {
                continue;
            }

            if (data->recoverAtDay > 0.0f) {
                if (now >= data->recoverAtDay) {
                    if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                        Defeat::RescueActor(actor, true);
                    }
                }
                continue;
            }

            // Held entry.
            if (data->leaseDeadlineDay <= 0.0f) {
                if (id == 0x14 && watchdog > 0.0f) {
                    data->leaseDeadlineDay = RecoveryDeadlineForSeconds(watchdog);
                    if (data->CopyLeaseOwner().empty()) {
                        data->SetLeaseOwner("(unclaimed hold)");
                    }
                }
                continue;
            }
            if (now >= data->leaseDeadlineDay) {
                const auto owner = data->CopyLeaseOwner();
                REX::WARN("[OSFDefeat] watchdog: held down-state of {:08X} expired (owner '{}' never extended or rescued) - force-rescuing",
                    id, owner.empty() ? "(unclaimed hold)" : owner);
                if (auto* const actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                    Defeat::RescueActor(actor, true);
                } else {
                    DefeatRegistry::Remove(id);  // unresolvable: purge so the entry cannot strand the co-save
                }
            }
        }

        // Presentation self-heal: the engine down-state survives a save-load / cell change (co-save +
        // Set3D re-assert) but the OSF scene that poses and locks the player does not — without this
        // the player sits in raw kBleedOut, frozen and unposed, until recovery. Debounced inside.
        auto* const player = PlayerActor();
        if (player && Defeat::IsDefeated(player)) {
            Presentation::ReassertPlayerDown();
        }
    }

    void Install()
    {
        if (g_hooked.exchange(true)) {
            return;
        }
        g_hooked.store(InstallHook());
        StartRecoveryWorker();
    }
}