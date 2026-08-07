Scriptname OSFDefeatBasicPickedClean extends Quest
{ OSF Defeat - Basic Consequences: "Picked Clean".

  The harsher, rarer sibling of "Robbed and Left". They went through everything:
  a steeper cut of your credits AND your digipicks, all into the aggressor's
  inventory (hunt them down to recover it), and they left you bruised. Weight-
  competes with "Robbed and Left" against human aggressors; a longer cooldown
  keeps the bigger loss uncommon.

  Started natively by OSF Defeat; down-state arrives held + automatically leased, so the
  M1 watchdog covers any failure here. }

Float Property DownSeconds = 18.0 Auto Const
{ How long the player stays down after the beating. }

Float Property RobbedPercent = 25.0 Auto Const
{ Cut of the player's credits the aggressor takes (percent). }

Int Property RobbedMin = 50 Auto Const
{ Floor: they always take at least this much (if you have it). }

Int Property RobbedMax = 5000 Auto Const
{ Cap: never lose more than this in one defeat. }

Spell Property Wound Auto Const
{ An affliction ABILITY added as the player goes down. Ships filled with a
  vanilla contusions affliction (ENV_AFFL_Injury_Contusions_1) - treatable at
  any doctor. Vanilla applies afflictions via AddSpell, not Cast (they are
  ability spells). Clear it in xEdit/CK to disable the wound. }

Event OnQuestInit()
    RunConsequence()
EndEvent

Function RunConsequence()
    Actor akPlayer    = Game.GetPlayer()
    Actor akAggressor = OSFDefeat.GetLastPlayerAggressor()

    If akAggressor != None
        ; Credits: a steeper cut than a plain mugging.
        MiscObject kCredits = Game.GetCredits()
        Int iHave = akPlayer.GetItemCount(kCredits)
        Int iTake = ((iHave as Float) * RobbedPercent / 100.0) as Int
        If iTake < RobbedMin
            iTake = RobbedMin
        EndIf
        If iTake > RobbedMax
            iTake = RobbedMax
        EndIf
        If iTake > iHave
            iTake = iHave
        EndIf
        If iTake > 0
            akPlayer.RemoveItem(kCredits, iTake, true, akAggressor)
        EndIf

        ; Digipicks: the signature "picked clean" touch - they took your picks.
        MiscObject kDigipick = Game.GetFormFromFile(0x0000000A, "Starfield.esm") as MiscObject
        If kDigipick != None
            Int iPicks = akPlayer.GetItemCount(kDigipick)
            If iPicks > 0
                akPlayer.RemoveItem(kDigipick, iPicks, true, akAggressor)
            EndIf
        EndIf

        Debug.Notification("Picked clean: " + iTake + " credits and your digipicks taken.")
    EndIf

    If Wound != None
        akPlayer.AddSpell(Wound, true)
    EndIf

    Utility.Wait(DownSeconds)
    OSFDefeatBasicRecovery.FinishAftermath(akPlayer)

    ; Stop so OSF Defeat can Start us again on the next defeat.
    Self.Stop()
EndFunction
