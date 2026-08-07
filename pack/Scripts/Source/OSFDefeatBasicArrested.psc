Scriptname OSFDefeatBasicArrested extends Quest
{ OSF Defeat - Basic Consequences: "Taken Into Custody".

  Fires when SECURITY puts the player down (aggressor faction conditions in
  basic-arrested.json). Rather than dying to UC Security, you wake up in
  processing: the aggressor's own crime faction runs the vanilla arrest
  (Faction.SendPlayerToJail), which handles bounty clearing, time served, and
  the release location - pure vanilla feel, zero new systems.

  Started natively by OSF Defeat; down-state arrives held + automatically leased, so
  the M1 watchdog covers any failure here. }

Float Property CustodySeconds = 4.0 Auto Const
{ Beat between going down and the jail transition. }

Event OnQuestInit()
    RunConsequence()
EndEvent

Function RunConsequence()
    Actor akPlayer    = Game.GetPlayer()
    Actor akAggressor = OSFDefeat.GetLastPlayerAggressor()

    Faction kCrimeFaction = None
    If akAggressor != None
        kCrimeFaction = akAggressor.GetCrimeFaction()
    EndIf

    If kCrimeFaction == None
        ; No crime faction to arrest under (odd security NPC): fall back to a
        ; plain short recovery; the framework owns it from here.
        OSFDefeat.ScheduleAutomaticRecovery(akPlayer, 8.0)
        Self.Stop()
        Return
    EndIf

    Debug.Notification("Security takes you into custody.")
    Utility.Wait(CustodySeconds)

    ; End the down-state BEFORE the jail hand-over: SendPlayerToJail owns the
    ; player from here (location, sentence, release) and must not fight the
    ; bleedout/ghost state.
    If !OSFDefeatBasicRecovery.RequestRecoveryAndWait(akPlayer)
        ; Do not hand a still-downed player to the jail pipeline. Resume a short framework timer
        ; and let the watchdog remain the final safety net.
        OSFDefeat.ScheduleAutomaticRecovery(akPlayer, 2.0)
        Self.Stop()
        Return
    EndIf
    kCrimeFaction.SendPlayerToJail(abRemoveInventory = false)

    ; Stop so OSF Defeat can Start us again on the next defeat.
    Self.Stop()
EndFunction
