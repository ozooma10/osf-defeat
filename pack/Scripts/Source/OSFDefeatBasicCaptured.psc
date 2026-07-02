Scriptname OSFDefeatBasicCaptured extends Quest
{ OSF Defeat - Basic Consequences: "Taken Alive".

  Fires when a HUMAN puts you down with EM damage - a stun, not a kill (cause
  "em" in basic-captured.json). They took you alive on purpose: you are held
  briefly as a non-hostile captive (nobody finishes you off), they lift a
  ransom into their own inventory, and then you are released groggy but whole.
  This is the scenario that gives EM its own identity - a stunned target is not
  a beaten one.

  Started natively by OSF Defeat; the down-state arrives held + auto-claimed, so
  the M1 watchdog rescues the player if this script ever fails. }

Float Property CaptiveSeconds = 25.0 Auto Const
{ How long you are held before they cut you loose. }

Float Property RansomPercent = 20.0 Auto Const
{ Cut of your credits taken as ransom (percent) - steeper than a plain mugging. }

Int Property RansomMin = 50 Auto Const
{ Floor: they always take at least this much (if you have it). }

Int Property RansomMax = 5000 Auto Const
{ Cap: never lose more than this in one capture. }

Event OnQuestInit()
    RunConsequence()
EndEvent

Function RunConsequence()
    Actor akPlayer    = Game.GetPlayer()
    Actor akAggressor = OSFDefeat.GetLastPlayerAggressor()

    ; Held as a captive: add the player to the configured captive faction so no
    ; nearby hostile executes them while they are down and helpless.
    OSFDefeat.ApplyCaptiveFaction(akPlayer)

    ; --- Ransom: a steeper cut, into the captor's inventory (recoverable). ---
    If akAggressor != None
        MiscObject kCredits = Game.GetCredits()
        Int iHave = akPlayer.GetItemCount(kCredits)
        Int iTake = ((iHave as Float) * RansomPercent / 100.0) as Int
        If iTake < RansomMin
            iTake = RansomMin
        EndIf
        If iTake > RansomMax
            iTake = RansomMax
        EndIf
        If iTake > iHave
            iTake = iHave
        EndIf
        If iTake > 0
            akPlayer.RemoveItem(kCredits, iTake, true, akAggressor)
            Debug.Notification("Stunned and taken alive: " + iTake + " credits ransomed.")
        Else
            Debug.Notification("Stunned and taken alive.")
        EndIf
    EndIf

    ; --- The captivity beat, then hand the down-state back. ------------------
    Utility.Wait(CaptiveSeconds)
    OSFDefeat.RemoveCaptiveFaction(akPlayer)
    OSFDefeat.RescueActor(akPlayer, True)

    ; Stop so OSF Defeat can Start us again on the next defeat.
    Self.Stop()
EndFunction
