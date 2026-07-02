Scriptname OSFDefeatBasicScrapped extends Quest
{ OSF Defeat - Basic Consequences: "Left for Scrap".

  Fires when a robot or turret puts you down (aggressor keyword conditions in
  basic-scrapped.json). A machine has no robbery motive: it registers you as
  neutralized and disengages. The shortest beat in the pack - an energy burn and
  a quick recovery - because nothing is sticking around to loot you.

  Started natively by OSF Defeat; down-state arrives held + auto-claimed, so the
  M1 watchdog covers any failure here. }

Float Property DownSeconds = 8.0 Auto Const
{ How long you stay down before you recover - short; nothing is guarding you. }

Spell Property Wound Auto Const
{ An affliction ABILITY added as the player goes down. Ships filled with a
  vanilla burns affliction (ENV_AFFL_Injury_Burns_1) - treatable at any doctor.
  Vanilla applies afflictions via AddSpell, not Cast (they are ability spells).
  Clear it in xEdit/CK to disable the wound. }

Event OnQuestInit()
    RunConsequence()
EndEvent

Function RunConsequence()
    Actor akPlayer = Game.GetPlayer()

    If Wound != None
        akPlayer.AddSpell(Wound, true)
    EndIf
    Debug.Notification("The unit registers you as neutralized and disengages.")

    Utility.Wait(DownSeconds)
    OSFDefeat.RescueActor(akPlayer, True)

    ; Stop so OSF Defeat can Start us again on the next defeat.
    Self.Stop()
EndFunction
