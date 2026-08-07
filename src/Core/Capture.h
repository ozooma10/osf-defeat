#pragma once

#include "pch.h"

#include "Core/DefeatRegistry.h"

namespace OSFDefeat::Capture
{
    // Capture hooks (M4): a persistent captor -> captive relationship layered on top of a defeat.
    // Distinct from the pieces it composes:
    //   - Defeat state (DefeatRegistry) is the down-state itself; a capture requires it and holds it.
    //   - Captive faction (Engine) is the pacify membership; a capture applies it so the captive
    //     survives among hostiles while held.
    //   - The ownership lease (ClaimDefeat) is a watchdog safety net; a capture is content intent
    //     ("this NPC is my prisoner") and is what a bounty board enumerates.
    // The framework provides the verbs and the bookkeeping; content provides the bounty board, the
    // ransom economy, and the brig. All mutating entry points run on the game thread.

    // Claim a_captive as held by a_captor. Defeats the captive first if it is not already down
    // (scripted cause, a_captor as aggressor), holds recovery indefinitely, and applies the captive
    // faction so nearby hostiles ignore it. Re-capturing updates the captor. a_captor may be null
    // (an un-attributed capture, e.g. "the station holds this prisoner"). Returns false if the
    // captive cannot be put down or the hold/pacify transaction cannot be established.
    bool CaptureActor(RE::Actor* a_captive, RE::Actor* a_captor);

    // End the capture. Removes the capture record and the captive faction; if a_recover, also
    // rescues the captive (ends the down-state). Returns true if a_actor was a captive.
    bool ReleaseCaptive(RE::Actor* a_actor, bool a_recover);

    // Move a captive to a target reference (a brig marker, a ship, a cell) via ObjectReference.MoveTo,
    // VM-dispatched because MoveTo is unbound in this CommonLibSF (same path as the quest handoff).
    // The Set3D re-assert hook keeps the down-state coherent at the destination. Refused if a_actor
    // is not a captive. RUNTIME-VERIFICATION PENDING: the MoveTo dispatch is unproven in-game.
    bool TransferCaptive(RE::Actor* a_actor, RE::TESObjectREFR* a_target);

    [[nodiscard]] bool     IsCaptive(RE::Actor* a_actor);
    [[nodiscard]] FormID   GetCaptorID(RE::Actor* a_actor);
    // Captives held by a_captor; a_captor == nullptr returns every captive. Resolvable actors only.
    [[nodiscard]] std::vector<RE::Actor*> GetCaptives(RE::Actor* a_captor);

    // Recovery notification: called from the rescue path so a captive that recovers (naturally,
    // by watchdog, or by ForceRecoverAll) stops being a captive. Idempotent.
    void NotifyRecovered(FormID a_captiveId);

    // Co-save support: captive, captor, captured day, and whether capture introduced the faction.
    [[nodiscard]] std::vector<std::tuple<FormID, FormID, float, bool>> Snapshot();
    void RestoreCapture(FormID a_captiveId, FormID a_captorId, float a_capturedDay, bool a_removeFactionOnRelease);
    void Clear();
}
