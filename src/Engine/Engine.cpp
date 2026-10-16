#include "pch.h"

#include "Engine/Engine.h"

#include "Core/Config.h"
#include "Core/RuntimeMutation.h"

#include "RE/B/BSPointerHandle.h"
#include "RE/P/ProcessLists.h"
#include "RE/T/TESFaction.h"

namespace OSFDefeat::Engine
{
    namespace
    {
        // Per-actor record of the captive-faction membership WE created (actorID -> factionID)
        // tracked ONLY when we actually add the membership, so removal deletes precisely what we introduced  never a pre-existing or a retargeted membership. 
        // Presence also doubles as the once-per-down guard against re-entrant adds (SetPlayerDownState re-runs on 3D re-attach / save-load re-assert). 
        // Persisted to the co-save so a load mid-captivity still removes exactly ours.
        std::map<std::uint32_t, std::uint32_t> g_captiveApplied;
        std::mutex g_captiveLock;

		[[nodiscard]] RE::TESNPC* NpcOf(RE::Actor& a_actor)
		{
			const auto base = a_actor.GetBaseObject();
			return base ? base->As<RE::TESNPC>() : nullptr;
		}

        [[nodiscard]] RE::ACTOR_LIFE_STATE LifeStateOf(RE::Actor& a_actor)
        {
            const auto state = (static_cast<RE::ActorState&>(a_actor).actorState1 >> 17) & 0xF;
            return static_cast<RE::ACTOR_LIFE_STATE>(state);
        }

        void SnapshotPriorActorState(RE::Actor& a_actor, ActiveDefeat& a_defeat)
        {
            if (a_defeat.priorState.valid) {
                return;
            }
            auto& prior = a_defeat.priorState;
            prior.aiEnabled = a_actor.boolBits.all(RE::Actor::BOOL_BITS::kProcessMe);
            prior.noBleedoutRecovery = a_actor.boolFlags.all(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
            prior.canSpeakToEssentialDown = a_actor.boolFlags.all(RE::Actor::BOOL_FLAGS::kCanSpeakToEssentialDown);
            prior.ghost = a_actor.extraDataList && a_actor.extraDataList->HasType(RE::ExtraDataType::kGhost);
            prior.actorEssential = a_actor.boolFlags.all(RE::Actor::BOOL_FLAGS::kEssential);
            if (auto* const npc = NpcOf(a_actor)) {
                prior.baseEssential = npc->IsEssential();
            }
            prior.lifeState = LifeStateOf(a_actor);
            prior.valid = true;  // publish last
        }

        void RestoreOwnedState(RE::Actor& a_actor, const ActorStateSnapshot& a_prior, bool a_isPlayer)
        {
            a_actor.boolFlags.set(a_prior.noBleedoutRecovery, RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
            if (a_isPlayer) {
                a_actor.boolFlags.set(a_prior.canSpeakToEssentialDown, RE::Actor::BOOL_FLAGS::kCanSpeakToEssentialDown);
            }
            a_actor.SetGhost(a_prior.ghost);
            a_actor.SetAIEnabled(a_prior.aiEnabled);
            a_actor.SetLifeState(a_prior.lifeState);
            if (a_isPlayer) {
                a_actor.boolFlags.set(a_prior.actorEssential, RE::Actor::BOOL_FLAGS::kEssential);
                if (auto* const npc = NpcOf(a_actor)) {
                    npc->actorData.actorBaseFlags.set(a_prior.baseEssential, RE::ACTOR_BASE_DATA::Flag::kEssential);
                }
            }
        }

        // Resolves the configured captive faction an actor joins so hostiles disengage. Returns nullptr when the FormID is 0 or it doesn't resolve to a TESFaction 
        [[nodiscard]] RE::TESFaction* CaptiveFaction()
        {
            const auto id = Config::CaptiveFactionID();
            return id ? RE::TESForm::LookupByID<RE::TESFaction>(id) : nullptr;
        }
    }
    RE::ActorValueInfo* HealthAV()
    {
        const auto* const avs = RE::ActorValue::GetSingleton();
        return avs ? avs->health : nullptr;
    }

    RE::ActorValueOwner& AVOwnerOf(RE::Actor& a_actor)
    {
        return static_cast<RE::ActorValueOwner&>(static_cast<RE::TESObjectREFR&>(a_actor));
    }

    RE::Actor* ResolveCombatTarget(RE::Actor* a_victim)
    {
        if (!a_victim) {
            return nullptr;
        }

        // currentCombatTarget is a raw TESPointerHandle in this CommonLibSF fork; reinterpret it
        // as the typed handle and resolve via GetSmartPointer (ID 35638). The age-checked manager
        // returns null for a stale/empty handle, so this is safe on the game thread.
        const auto& handle = reinterpret_cast<const RE::BSPointerHandle<RE::Actor>&>(a_victim->currentCombatTarget);
        if (!handle) {
            return nullptr;
        }
        const RE::NiPointer<RE::Actor> target = handle.get();
        return target.get();
    }

    namespace
    {
        void RestoreHealthToMax(RE::Actor& a_actor)
        {
            auto* const av = HealthAV();
            if (!av) {
                return;
            }
            auto& owner = AVOwnerOf(a_actor);
            const float maxHealth = std::max(owner.GetPermanentActorValue(*av), 1.0f);
            const float current = owner.GetActorValue(*av);
            if (current < maxHealth) {
                owner.RestoreActorValue(*av, maxHealth - current);
            }
            if (owner.GetActorValue(*av) < 1.0f) {
                owner.SetActorValue(*av, maxHealth);
            }
        }

        void SetPlayerDownState(RE::Actor& a_actor)
        {
            if (auto* const npc = NpcOf(a_actor)) {
                a_actor.boolFlags.set(RE::Actor::BOOL_FLAGS::kEssential);
                npc->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kEssential);
            }
            RestoreHealthToMax(a_actor);
            a_actor.boolFlags.set(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
            a_actor.boolFlags.set(RE::Actor::BOOL_FLAGS::kCanSpeakToEssentialDown);
            a_actor.SetGhost(true);
            // NOTE: no SetAIEnabled(false) for the player — kProcessMe-off can suppress the player's camera/
            // look (it's the NPC lever per OSF RE). The OSF Animation scene's input lock (movement/combat off,
            // looking free) handles "can't act".
            a_actor.SetLifeState(kBleedOut);
            a_actor.StopCombat();
            // Drop the downed player from every combatant's target list — the same manager-level lever
            // SetNpcDownState uses. Attackers disengage FROM THE PLAYER but stay in combat and keep fighting other
            // hostiles (your allies / nearby NPCs). Unlike the calm faction below, this does NOT make them
            // non-hostile, so they still re-aggro whoever is actually attacking them, and the downed player is
            // simply no longer a valid target. Ghost (set above) keeps the player invulnerable + de-prioritized
            // meanwhile; on recovery the player is re-detected through normal combat scanning (no tcai gate in
            // play, so combat controllers keep ticking — that gate was the freeze cause in the drop-pacify history).
            if (auto* const lists = RE::ProcessLists::GetSingleton()) {
                lists->StopCombatAndAlarmOnActor(&a_actor);
                REX::INFO("[OSFDefeat] player-down: dropped from combatant target lists (attackers keep fighting other hostiles)");
            }
            // OPTIONAL captive-faction neutrality (`applyCaptiveFactionOnPlayerDefeat`, default OFF): add the
            // player to a captive faction so attackers treat them as non-hostile. It disengages enemies from
            // EVERYONE — they stop fighting your allies too — which is why it's opt-in rather than the default.
            // ApplyCaptiveFaction carries the guards (once per down, never over a pre-existing membership).
            if (Config::ShouldApplyCaptiveFactionOnPlayerDefeat()) {
                ApplyCaptiveFaction(a_actor);
            }
        }

        void SetNpcDownState(RE::Actor& a_actor)
        {
            // Live-validated per-actor recipe (OSF RE npc-defeat-recover-pacify, 2026-06-25): bleedout +
            // ghost (invulnerable so the defeat sticks) + manager-level combat removal so other attackers
            // disengage. The actor is left at its (clamped/current) HP; ghost makes it damage-immune.
            a_actor.boolFlags.set(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
            a_actor.SetGhost(true);
            a_actor.SetLifeState(kBleedOut);
            a_actor.StopCombat();
            if (auto* const lists = RE::ProcessLists::GetSingleton()) {
                lists->StopCombatAndAlarmOnActor(&a_actor);
            }
        }
    }

    void ApplyDownState(RE::Actor& a_actor, ActiveDefeat& a_defeat)
    {
        SnapshotPriorActorState(a_actor, a_defeat);
        if (a_actor.IsPlayerRef()) {
            SetPlayerDownState(a_actor);
        } else {
            SetNpcDownState(a_actor);
        }
    }

    void RestoreState(RE::Actor& a_actor, const ActiveDefeat& a_defeat)
    {
        RestoreHealthToMax(a_actor);
        RestoreOwnedState(a_actor, a_defeat.priorState, a_actor.IsPlayerRef());
        // EvaluatePackage is intentionally NOT called: its AddressLib ID is unbound (0) in commonlibsf
        // (candidate 150640, never verified), so invoking it jumps to base+0 and crashes ("AddressLib
        // resolved value 0"). AI resumes on the next think after SetAIEnabled(true); a forced package
        // re-eval isn't required for recovery. Re-add only once 150640 is RE-proven for the live build.
        // Remove exactly the faction we added, preserving pre-existing memberships even if config changed.
        RemoveCaptiveFaction(a_actor);
    }

    bool ApplyCaptiveFaction(RE::Actor& a_actor)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        auto* const faction = CaptiveFaction();
        if (!faction) {
            REX::WARN("[OSFDefeat] captive-faction application skipped: captiveFaction unresolved");
            return false;
        }

        const std::uint32_t actorId = a_actor.GetFormID();
        {
            std::unique_lock lock{ g_captiveLock };
            if (g_captiveApplied.contains(actorId)) {
                return false;  // already ours; re-entrant apply (3D re-attach, verb retry) is a no-op
            }
        }
        // AddToFaction is create-or-update and RemoveFromFaction deletes outright, so never add over a
        // legitimate pre-existing membership — a later removal would destroy it. Not tracking it means
        // removal correctly leaves it alone.
        if (a_actor.GetFactionRank(faction, a_actor.IsPlayerRef()) >= 0) {
            return false;
        }

        a_actor.AddToFaction(faction, 0);
        {
            std::unique_lock lock{ g_captiveLock };
            g_captiveApplied[actorId] = faction->GetFormID();
        }
        REX::INFO("[OSFDefeat] captive faction: {:08X} added to faction {:08X}", actorId, faction->GetFormID());
        return true;
    }

    bool RemoveCaptiveFaction(RE::Actor& a_actor)
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };

        std::uint32_t applied = 0;
        {
            std::unique_lock lock{ g_captiveLock };
            if (const auto it = g_captiveApplied.find(a_actor.GetFormID()); it != g_captiveApplied.end()) {
                applied = it->second;
                g_captiveApplied.erase(it);
            }
        }
        if (applied == 0) {
            return false;
        }
        if (auto* const faction = RE::TESForm::LookupByID<RE::TESFaction>(applied)) {
            a_actor.RemoveFromFaction(faction);
            REX::INFO("[OSFDefeat] captive faction: {:08X} removed from faction {:08X}", a_actor.GetFormID(), applied);
        }
        return true;
    }

    bool HasCaptiveFactionApplied(RE::Actor& a_actor)
    {
        std::unique_lock lock{ g_captiveLock };
        return g_captiveApplied.contains(a_actor.GetFormID());
    }

    bool IsInCaptiveFaction(RE::Actor& a_actor)
    {
        auto* const faction = CaptiveFaction();
        return faction && a_actor.GetFactionRank(faction, a_actor.IsPlayerRef()) >= 0;
    }

    bool IsCaptiveFactionAvailable()
    {
        return CaptiveFaction() != nullptr;
    }

    std::vector<std::pair<std::uint32_t, std::uint32_t>> SnapshotCaptiveFactions()
    {
        std::unique_lock lock{ g_captiveLock };
        return { g_captiveApplied.begin(), g_captiveApplied.end() };
    }

    void RestoreCaptiveFaction(std::uint32_t a_actorId, std::uint32_t a_factionId)
    {
        // Re-track only: the membership itself is already in the save. Restoring the record is what
        // lets the eventual recovery remove exactly the membership we created before the save.
        if (a_actorId && a_factionId) {
            std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
            std::unique_lock lock{ g_captiveLock };
            g_captiveApplied[a_actorId] = a_factionId;
        }
    }

    void ResetRuntimeState()
    {
        std::scoped_lock stateLock{ RuntimeMutation::Mutex() };
        std::unique_lock lock{ g_captiveLock };
        g_captiveApplied.clear();
    }
}
