#include "pch.h"

#include "Engine/Engine.h"

#include "Core/Config.h"

#include "RE/B/BSPointerHandle.h"
#include "RE/P/ProcessLists.h"
#include "RE/T/TESFaction.h"

namespace OSFDefeat::Engine
{
    namespace
    {
        std::atomic_bool g_hasEssentialSnapshot{ false };
        bool g_wasEssential{ false };

        // FormID of the "captive" faction we added the downed player to (SAF Surrender-style pacify), or 0
        // when we didn't add one (pacify disabled, unresolved faction, or the player was ALREADY a member so
        // we left their real membership untouched). It is set ONLY when we actually create the membership, so
        // recovery removes precisely what we introduced — never a pre-existing or a retargeted membership.
        // A nonzero value also doubles as the once-per-down guard against re-entrant SetPlayerDownState adds.
        std::atomic<std::uint32_t> g_captiveFactionApplied{ 0 };

        [[nodiscard]] bool PlayerIsEssential(RE::Actor* a_actor)
        {
            auto* const npc = a_actor ? a_actor->GetNPC() : nullptr;
            return npc ? npc->IsEssential() : false;
        }

        // Resolves the configured captive faction the downed player joins so hostiles disengage. Returns
        // nullptr when faction-pacify is disabled, the FormID is 0, or it doesn't resolve to a TESFaction —
        // callers then simply skip the pacify (leaving Ghost as the only protection).
        [[nodiscard]] RE::TESFaction* CaptiveFaction()
        {
            if (!Config::IsFactionPacifyEnabled()) {
                return nullptr;
            }
            const auto id = Config::CaptiveFactionID();
            return id ? RE::TESForm::LookupByID<RE::TESFaction>(id) : nullptr;
        }
    }
    RE::ActorValueInfo* HealthAV()
    {
        const auto* const avs = RE::ActorValue::GetSingleton();
        return avs ? avs->health : nullptr;
    }

    RE::ActorValueInfo* EMHealthAV()
    {
        // EM/stun damage depletes a separate "Electromagnetic Health" gauge through the same
        // ModActorValue primitive as Health (OSF RE em-stun-damage-routing, 2026-06-25). This is
        // the typed equivalent of the proven BGSDefaultObjectManager getter (ID 43134) + 0xD0.
        const auto* const avs = RE::ActorValue::GetSingleton();
        return avs ? avs->electromagneticHealth : nullptr;
    }

    RE::ActorValueOwner* AVOwnerOf(RE::Actor* a_actor)
    {
        return a_actor ? static_cast<RE::ActorValueOwner*>(static_cast<RE::TESObjectREFR*>(a_actor)) : nullptr;
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

    float GetHealth(RE::Actor* a_actor)
    {
        auto* const av = HealthAV();
        auto* const owner = AVOwnerOf(a_actor);
        return av && owner ? owner->GetActorValue(*av) : 0.0f;
    }

    float GetMaxHealth(RE::Actor* a_actor)
    {
        auto* const av = HealthAV();
        auto* const owner = AVOwnerOf(a_actor);
        return av && owner ? owner->GetPermanentActorValue(*av) : 0.0f;
    }

    void RestoreHealthToMax(RE::Actor* a_actor)
    {
        auto* const av = HealthAV();
        auto* const owner = AVOwnerOf(a_actor);
        if (!av || !owner) {
            return;
        }

        const float maxHealth = std::max(owner->GetPermanentActorValue(*av), 1.0f);
        const float current = owner->GetActorValue(*av);
        if (current < maxHealth) {
            owner->RestoreActorValue(*av, maxHealth - current);
        }
        if (owner->GetActorValue(*av) < 1.0f) {
            owner->SetActorValue(*av, maxHealth);
        }
    }

    void SetPlayerEssential(bool a_essential)
    {
        auto* const player = RE::PlayerCharacter::GetSingleton();
        auto* const actor = static_cast<RE::Actor*>(player);
        auto* const npc = actor ? actor->GetNPC() : nullptr;
        if (!actor || !npc) {
            return;
        }

        actor->boolFlags.set(a_essential, RE::Actor::BOOL_FLAGS::kEssential);
        npc->actorData.actorBaseFlags.set(a_essential, RE::ACTOR_BASE_DATA::Flag::kEssential);
    }

    void SetPlayerDownState(RE::Actor* a_actor)
    {
        if (!a_actor || !a_actor->IsPlayerRef()) {
            return;
        }

        bool expected = false;
        if (g_hasEssentialSnapshot.compare_exchange_strong(expected, true)) {
            g_wasEssential = PlayerIsEssential(a_actor);
        }
        SetPlayerEssential(true);
        RestoreHealthToMax(a_actor);
        a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
        a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kCanSpeakToEssentialDown);
        a_actor->SetGhost(true);
        // NOTE: no SetAIEnabled(false) for the player — kProcessMe-off can suppress the player's camera/
        // look (it's the NPC lever per OSF RE). The OSF Animation scene's input lock (movement/combat off,
        // looking free) handles "can't act".
        a_actor->SetLifeState(kBleedOut);
        a_actor->StopCombat();
        // Drop the downed player from every combatant's target list — the same manager-level lever
        // SetNpcDownState uses. Attackers disengage FROM THE PLAYER but stay in combat and keep fighting other
        // hostiles (your allies / nearby NPCs). Unlike the calm faction below, this does NOT make them
        // non-hostile, so they still re-aggro whoever is actually attacking them, and the downed player is
        // simply no longer a valid target. Ghost (set above) keeps the player invulnerable + de-prioritized
        // meanwhile; on recovery the player is re-detected through normal combat scanning (no tcai gate in
        // play, so combat controllers keep ticking — that gate was the freeze cause in the drop-pacify history).
        if (auto* const lists = RE::ProcessLists::GetSingleton()) {
            lists->StopCombatAndAlarmOnActor(a_actor);
            REX::INFO("[OSFDefeat] player-down: dropped from combatant target lists (attackers keep fighting other hostiles)");
        }
        // OPTIONAL full "surrender" calm (SAF Surrender style, `factionPacify`, default OFF): also add the
        // player to a captive faction so attackers treat them as non-hostile. It disengages enemies from
        // EVERYONE — they stop fighting your allies too — which is why it's opt-in rather than the default.
        // Guarded to add at most ONCE per down (g_captiveFactionApplied still 0) and ONLY if the player isn't
        // already a member (GetFactionRank < 0 = non-member). AddToFaction is create-or-update and
        // RemoveFromFaction deletes outright, so a blind add+remove would clobber then destroy a legitimate
        // pre-existing membership; and SetPlayerDownState is re-entrant (3D re-attach via Reattach, save-load
        // re-assert via Defeat::RestoreFromSave), so a blind re-add could stack a second faction after a
        // mid-down captiveFaction change and leak the first. Skipping when already a member leaves
        // g_captiveFactionApplied at 0, so recovery correctly won't remove a membership we didn't create.
        if (g_captiveFactionApplied.load() == 0) {
            if (auto* const faction = CaptiveFaction(); faction && a_actor->GetFactionRank(faction, true) < 0) {
                a_actor->AddToFaction(faction, 0);
                g_captiveFactionApplied.store(faction->GetFormID());
                REX::INFO("[OSFDefeat] captive-faction pacify: player added to faction {:08X}", faction->GetFormID());
            }
        }
    }

    void RestorePlayerState(RE::Actor* a_actor)
    {
        if (!a_actor || !a_actor->IsPlayerRef()) {
            return;
        }

        RestoreHealthToMax(a_actor);
        a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
        a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kCanSpeakToEssentialDown);
        a_actor->SetGhost(false);
        a_actor->SetAIEnabled(true);
        a_actor->SetLifeState(kAlive);
        // EvaluatePackage is intentionally NOT called: its AddressLib ID is unbound (0) in commonlibsf
        // (candidate 150640, never verified), so invoking it jumps to base+0 and crashes ("AddressLib
        // resolved value 0"). AI resumes on the next think after SetAIEnabled(true); a forced package
        // re-eval isn't required for recovery. Re-add only once 150640 is RE-proven for the live build.
        if (g_hasEssentialSnapshot.exchange(false)) {
            SetPlayerEssential(g_wasEssential);
        }
        // Undo the captive-faction pacify from SetPlayerDownState: remove exactly the faction we added
        // (tracked by FormID so a config change mid-down can't retarget the removal, and a pre-existing
        // membership we didn't add is left untouched). Normal AI re-hostiles the enemies on the next think;
        // unghosting above lets their fire land again.
        if (const std::uint32_t applied = g_captiveFactionApplied.exchange(0); applied != 0) {
            if (auto* const faction = RE::TESForm::LookupByID<RE::TESFaction>(applied)) {
                a_actor->RemoveFromFaction(faction);
                REX::INFO("[OSFDefeat] captive-faction pacify: player removed from faction {:08X}", applied);
            }
        }
    }

    void SetNpcDownState(RE::Actor* a_actor)
    {
        if (!a_actor || a_actor->IsPlayerRef()) {
            return;
        }

        // Live-validated per-actor recipe (OSF RE npc-defeat-recover-pacify, 2026-06-25): bleedout +
        // ghost (invulnerable so the defeat sticks) + manager-level combat removal so other attackers
        // disengage. The actor is left at its (clamped/current) HP; ghost makes it damage-immune.
        a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
        a_actor->SetGhost(true);
        a_actor->SetLifeState(kBleedOut);
        a_actor->StopCombat();
        if (auto* const lists = RE::ProcessLists::GetSingleton()) {
            lists->StopCombatAndAlarmOnActor(a_actor);
        }
    }

    void RestoreNpcState(RE::Actor* a_actor)
    {
        if (!a_actor || a_actor->IsPlayerRef()) {
            return;
        }

        RestoreHealthToMax(a_actor);
        a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kNoBleedoutRecovery);
        a_actor->SetGhost(false);
        a_actor->SetLifeState(kAlive);
        a_actor->SetAIEnabled(true);
        // EvaluatePackage is intentionally NOT called: its AddressLib ID is unbound (0) in commonlibsf
        // (candidate 150640, never verified), so invoking it jumps to base+0 and crashes ("AddressLib
        // resolved value 0"). AI resumes on the next think after SetAIEnabled(true); a forced package
        // re-eval isn't required for recovery. Re-add only once 150640 is RE-proven for the live build.
    }

    void ApplyDownState(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return;
        }
        if (a_actor->IsPlayerRef()) {
            SetPlayerDownState(a_actor);
        } else {
            SetNpcDownState(a_actor);
        }
    }

    void RestoreState(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return;
        }
        if (a_actor->IsPlayerRef()) {
            RestorePlayerState(a_actor);
        } else {
            RestoreNpcState(a_actor);
        }
    }
}