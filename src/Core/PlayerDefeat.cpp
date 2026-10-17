#include "pch.h"

#include "Core/PlayerDefeat.h"

#include "Core/Config.h"
#include "Core/Consequence.h"
#include "Core/Decision.h"
#include "Core/Defeat.h"
#include "Core/DefeatContext.h"
#include "Core/Events.h"
#include "Core/DefeatRegistry.h"
#include "Core/QuestHandoff.h"
#include "Core/Recovery.h"
#include "Core/Truce.h"
#include "Core/RuntimeMutation.h"
#include "Core/Eligibility.h"
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
        std::atomic_bool g_pendingDefeat{ false };

        // --- Recent hostile-attacker recording -----------------------------------------------------
        // Who damaged the player recently, fed passively by the hook below. This is the recent-attacker
        // source for the defeat-time consequence cast: live ProcessLists actor arrays must NOT be iterated from
        // an SFSE task (proven crash: races the sim thread's realloc → AV in GetSmartPointer), so we
        // only store PODs here (FormID + timestamp, worker thread) and resolve via LookupByID on the
        // game thread later. "Damaged the player in the last window" is also the better cast list —
        // actual combatants, not bystanders.
        constexpr auto kRecentAttackerWindow = std::chrono::seconds{ 30 };
        constexpr std::size_t kRecentAttackerCap = 64;
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
            std::erase_if(g_recentAttackers, [&](const auto& entry) { return now - entry.second > kRecentAttackerWindow; });
            if (g_recentAttackers.size() < kRecentAttackerCap || g_recentAttackers.contains(id)) {
                g_recentAttackers[id] = now;
            }
        }

        [[nodiscard]] std::vector<FormID> RecentAttackerIDs()
        {
            const auto now = std::chrono::steady_clock::now();
            std::vector<FormID> out;
            std::unique_lock lock{ g_recentAttackersLock };
            for (const auto& [id, when] : g_recentAttackers) {
                if (now - when <= kRecentAttackerWindow) {
                    out.push_back(id);
                }
            }
            return out;
        }

        // Defeat-time hostile-attacker snapshot: the aggressor plus every recent attacker that is still a
        // live hostile within a_radius (<= 0 = no radius filter). Game thread (LookupByID + engine
        // reads). Recorded attackers that resolve to non-actors (traps, hazards) drop out here.
        [[nodiscard]] std::vector<RE::Actor*> CollectRecentHostileAttackers(RE::Actor& a_player, RE::Actor* a_aggressor, float a_radius)
        {
            std::vector<RE::Actor*> out;
            const auto origin = a_player.GetPosition();
            const float radiusSq = a_radius > 0.0f ? a_radius * a_radius : 0.0f;
            const auto consider = [&](RE::Actor* a_actor, bool a_requireHostile) {
                if (!a_actor || a_actor == &a_player || a_actor->IsDead()) {
                    return;
                }
                if (std::find(out.begin(), out.end(), a_actor) != out.end()) {
                    return;
                }
                if (a_requireHostile && !a_actor->IsHostileToActor(&a_player)) {
                    return;
                }
                if (radiusSq > 0.0f && (a_actor->GetPosition() - origin).SqrLength() > radiusSq) {
                    return;
                }
                out.push_back(a_actor);
            };

            // The aggressor is always included (no hostility re-check: captive-faction membership or the
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

        void QueueImminentBroadcast(RE::BSService::TaskQueue& a_tasks, std::uint64_t a_epoch)
        {
            a_tasks.AddTask([a_epoch]() {
                if (!RuntimeMutation::IsCurrent(a_epoch)) {
                    return;
                }
                auto* const player = static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton());
                if (player && !Defeat::IsDefeated(player)) {
                    Events::BroadcastPlayerDefeatImminent();
                }
            });
        }

        [[nodiscard]] bool ShouldSignalImminent(RE::ActorValueOwner* a_owner, const RE::ActorValueInfo& a_info, float a_current, float a_delta)
        {
            const float fraction = Config::ImminentHealthFraction();
            if (fraction <= 0.0f) {
                return false;
            }
            const float maxValue = a_owner->GetPermanentActorValue(a_info);
            if (maxValue <= 0.0f) {
                return false;
            }
            if (a_current + a_delta <= maxValue * fraction) {
                return !g_imminentSignaled.exchange(true);
            } else {
                g_imminentSignaled.store(false);  // back above the threshold: re-arm
            }
            return false;
        }

        [[nodiscard]] RE::Actor* PlayerActor()
        {
            return static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton());
        }

        [[nodiscard]] bool IsPlayerAVOwner(RE::ActorValueOwner* a_owner)
        {
            auto* const player = PlayerActor();
            return player && a_owner == &Engine::AVOwnerOf(*player);
        }

        void QueueBegin(RE::BSService::TaskQueue& a_tasks, FormID a_aggressorId, DefeatCause a_cause, std::uint64_t a_epoch)
        {
            a_tasks.AddTask([a_aggressorId, a_cause, a_epoch]() {
                    if (!RuntimeMutation::IsCurrent(a_epoch)) {
                        // Revert owns resetting the latch. An old callback must not clear a
                        // newly accepted defeat in the next save.
                        return;
                    }
                    auto* aggressor = a_aggressorId ? RE::TESForm::LookupByID<RE::Actor>(a_aggressorId) : nullptr;
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
        }

        // Health reaches a terminal state (death) through the ModActorValue primitive: we clamp the
        // would-be-lethal Health delta at a floor so that terminal path never fires.
        [[nodiscard]] bool IsLethalGauge(const RE::ActorValueInfo& a_info)
        {
            return &a_info == Engine::HealthAV();
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

        void QueueNpcDefeat(RE::BSService::TaskQueue& a_tasks, FormID a_victimId, FormID a_aggressorId, DefeatCause a_cause, std::uint64_t a_epoch)
        {
            a_tasks.AddTask([a_victimId, a_aggressorId, a_cause, a_epoch]() {
                if (!RuntimeMutation::IsCurrent(a_epoch)) {
                    return;
                }
                auto* const victim = a_victimId ? RE::TESForm::LookupByID<RE::Actor>(a_victimId) : nullptr;
                if (!victim || Defeat::IsDefeated(victim)) {
                    return;
                }
                auto* aggressor = a_aggressorId ? RE::TESForm::LookupByID<RE::Actor>(a_aggressorId) : nullptr;
                if (!aggressor) {
                    aggressor = Engine::ResolveCombatTarget(victim);
                }
                // The damage hook validated this hit before accepting/clamping it. Treat the queued
                // operation as a commit; re-evaluating mutable config here could strand the actor at
                // the health floor without either vanilla death or an OSF down-state.
                if (Defeat::DefeatActor(victim, aggressor, a_cause)) {
                    if (auto data = DefeatRegistry::Get(a_victimId)) {
                        data->autoRecoverAtDay = Recovery::DeadlineForSeconds(Config::DefaultAutoRecoveryDelaySeconds());
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
            // Remember only this call's post-write action. Nothing is submitted until the
            // original returns: native AddTask may execute inline on its drain owner.
            enum class AfterDamage { kNone, kImminent, kPlayerDefeat, kNpcDefeat };
            auto action = AfterDamage::kNone;
            RE::BSService::TaskQueue* tasks = nullptr;
            FormID victimId = 0;
            const auto epoch = RuntimeMutation::CurrentEpoch();
            // Outgoing health damage ends the attacker's truce even when NPC auto-defeat
            // is disabled. Only IDs cross the worker/game-thread boundary.
            const FormID attackingId = a_ref && a_modifier == RE::ACTOR_VALUE_MODIFIER::kDamage &&
                a_delta < 0.0f && IsLethalGauge(a_info) ? a_ref->GetFormID() : 0;
            const bool breakTruce = attackingId && Truce::IsActive(attackingId);

            if (a_modifier == RE::ACTOR_VALUE_MODIFIER::kDamage && a_delta < 0.0f && IsLethalGauge(a_info)) {
                if (IsPlayerAVOwner(a_owner)) {
                    RecordAttacker(a_ref);  // recent-hostile-attacker snapshot source (POD-only on this thread)
                    auto* const player = PlayerActor();
                    const float current = a_owner->GetActorValue(a_info);
                    const bool  lethal = current + a_delta <= 0.0f;
                    if (Defeat::IsDamageImmune(player)) {
                        a_delta = 0.0f;
                    } else {
                        if (!lethal) {
                            if (ShouldSignalImminent(a_owner, a_info, current, a_delta)) {
                                tasks = RuntimeMutation::GetTaskQueue();
                                if (tasks) {
                                    action = AfterDamage::kImminent;
                                }
                            }
                        } else {
                            const auto verdict = Eligibility::Explain(player);
                            if (verdict != Eligibility::Verdict::kOk) {
                                LogPlayerPassthrough(Eligibility::VerdictName(verdict));
                            } else if (ResistWindowActive(player->GetFormID())) {
                                LogPlayerPassthrough("resist window active (ResistDefeat) - vanilla lethality");
                            } else {
                                tasks = RuntimeMutation::GetTaskQueue();
                                if (tasks) {
                                    // Reserve before the health write so another lethal hit
                                    // clamps too without scheduling a duplicate defeat.
                                    if (!g_pendingDefeat.exchange(true)) {
                                        action = AfterDamage::kPlayerDefeat;
                                    }
                                    a_delta = std::max(a_delta, kLethalHealthFloor - current);
                                } else {
                                    LogPlayerPassthrough("native task queue unavailable");
                                }
                            }
                        }
                    }
                } else if (Config::IsNpcAutoDefeatEnabled()) {
                    auto* const victim = ActorFromAVOwner(a_owner);
                    if (victim && Defeat::IsDamageImmune(victim)) {
                        a_delta = 0.0f;
                    } else if (victim && !victim->IsPlayerRef()) {
                        const float current = a_owner->GetActorValue(a_info);
                        const bool  lethal = current + a_delta <= 0.0f;
                        if (lethal && !Defeat::IsDefeated(victim)) {
                            const auto verdict = Eligibility::Explain(victim);
                            if (verdict != Eligibility::Verdict::kOk) {
                                if (Config::IsExplainEnabled()) {
                                    REX::INFO("[OSFDefeat] lethal NPC hit on {:08X} passed through: {}",
                                        victim->GetFormID(), Eligibility::VerdictName(verdict));
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
                                tasks = RuntimeMutation::GetTaskQueue();
                                if (tasks) {
                                    victimId = victim->GetFormID();
                                    action = AfterDamage::kNpcDefeat;
                                    a_delta = std::max(a_delta, kLethalHealthFloor - current);
                                } else if (Config::IsExplainEnabled()) {
                                    REX::INFO("[OSFDefeat] lethal NPC hit on {:08X} passed through: native task queue unavailable",
                                        victim->GetFormID());
                                }
                            }
                        }
                    }
                }
            }

            // Capture IDs before the engine call; never carry a raw actor/reference into a task.
            const FormID aggressorId = action != AfterDamage::kNone && a_ref ? a_ref->GetFormID() : 0;
            g_original(a_owner, a_modifier, a_info, a_delta, a_ref);

            if (breakTruce) {
                Truce::QueueEndForAttack(attackingId, epoch);
            }
            switch (action) {
            case AfterDamage::kPlayerDefeat:
                QueueBegin(*tasks, aggressorId, DefeatCause::kHealth, epoch);
                break;
            case AfterDamage::kNpcDefeat:
                QueueNpcDefeat(*tasks, victimId, aggressorId, DefeatCause::kHealth, epoch);
                break;
            case AfterDamage::kImminent:
                QueueImminentBroadcast(*tasks, epoch);
                break;
            case AfterDamage::kNone:
                break;
            }
        }
    }

    bool SetResistWindow(RE::Actor* a_actor, float a_seconds)
    {
        if (!a_actor || !std::isfinite(a_seconds)) {
            return false;
        }
        const FormID id = a_actor->GetFormID();
        if (a_seconds <= 0.0f) {
            std::unique_lock lock{ g_resistLock };
            g_resistWindows.erase(id);
            return true;
        }
        if (Defeat::IsDefeated(a_actor)) {
            return false;  // a resist window is not recovery
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

    void BeginPlayerDefeat(RE::Actor* a_aggressor, DefeatCause a_cause)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        auto* const player = PlayerActor();
        // The damage hook is the eligibility/commit boundary for Health defeats. Do not re-evaluate
        // mutable config after the hook has already clamped lethal damage.
        if (!player || player->IsDead() || Defeat::IsDefeated(player)) {
            return;
        }

        const auto consequence = Consequence::SelectPlayerConsequence(player, a_aggressor, a_cause);
        // Snapshot recent hostile attackers BEFORE the engine down state applies: DefeatActor drops the player from
        // combatant target lists (and may apply captive-faction membership), which can clear the hostility the
        // recent-hostile-attacker filter keys on.
        const auto recentHostileAttackers = CollectRecentHostileAttackers(*player, a_aggressor, Config::RecentHostileAttackerRadius());
        const auto equippedWeapon = Engine::PrimaryWeaponFormID(*player);
        if (!Defeat::DefeatActor(player, a_aggressor, a_cause)) {
            return;
        }
        Consequence::MarkFired(consequence, player);  // stamp cooldown / oncePerLocation arbitration state
        g_imminentSignaled.store(false);          // re-arm the defeat-imminent signal for the next fight

        if (auto data = DefeatRegistry::Get(player->GetFormID())) {
            if (consequence.HoldsRecovery()) {
                // Held for content: the consequence owns the down-state and must end it via
                // RecoverActor (or ScheduleAutomaticRecovery / ReleaseDefeatLease). The lease is armed on its
                // behalf so a consequence that errors out gets watchdog-recovered, not stranded.
                const float hold = consequence.leaseTimeoutSeconds > 0.0f ? consequence.leaseTimeoutSeconds : Config::WatchdogSeconds();
                data->autoRecoverAtDay = 0.0f;
                data->SetLeaseHolderId(std::format("consequence:{}", consequence.id));
                data->leaseExpiresAtDay = hold > 0.0f ? Recovery::DeadlineForSeconds(hold) : 0.0f;
            } else {
                data->autoRecoverAtDay = Recovery::DeadlineForSeconds(consequence.autoRecoveryDelaySeconds);
            }
        }

        if (consequence.HoldsRecovery()) {
            REX::INFO("[OSFDefeat] player consequence '{}' selected; recovery held for content", consequence.id);
        } else {
            REX::INFO("[OSFDefeat] player consequence '{}' selected; automatic recovery in {:.2f}s", consequence.id, consequence.autoRecoveryDelaySeconds);
        }

        // Stage the handoff payload (read back through the GetLastPlayer* Papyrus
        // getters) — including the recent-hostile-attacker snapshot (Acheron
        // member-list parity) — then start the consequence quest, then notify content
        // consumers such as OSF Surrender. Context-before-quest so a quest's
        // OnQuestInit can already read the payload. We are on the game thread here,
        // so the VM dispatches are safe.
        DefeatContext::SetPlayerContext(a_aggressor, consequence, recentHostileAttackers, a_cause, equippedWeapon);
        if (consequence.quest && consequence.startQuest) {
            QuestHandoff::TryStartQuestToken(*consequence.quest);
        }
        Events::BroadcastPlayerConsequenceStarted();
    }

    void Install()
    {
        const auto target = kModActorValueID.address();
        const auto* const bytes = reinterpret_cast<const std::uint8_t*>(target);
        if (!std::equal(kExpectedPrologue.begin(), kExpectedPrologue.end(), bytes)) {
            REX::ERROR("[OSFDefeat] ModActorValue prologue mismatch; player lethal hook disabled");
            return;
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
            return;
        }
        if constexpr (kStolenLength > sizeof(jmp)) {
            (void)REL::WriteSafeFill(target + sizeof(jmp), 0x90, kStolenLength - sizeof(jmp));
        }

        REX::INFO("[OSFDefeat] installed player lethal damage hook at {:X}", target);
    }

    void ResetTransientState()
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        g_pendingDefeat.store(false);
        g_imminentSignaled.store(false);
        {
            std::unique_lock lock{ g_recentAttackersLock };
            g_recentAttackers.clear();
        }
        {
            std::unique_lock lock{ g_resistLock };
            g_resistWindows.clear();
        }
    }
}
