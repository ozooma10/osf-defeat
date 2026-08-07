Scriptname OSFDefeatBasicMauled extends Quest
{ OSF Defeat - Basic Consequences: "Mauled".

  Fires when a creature puts you down (aggressor keyword conditions in
  basic-mauled.json). A predator loses interest once you stop moving and prowls
  off. The longest down beat in the pack and the deepest wound (lacerations);
  once-per-location so a den does not maul you on a loop the moment you recover.

  Started natively by OSF Defeat; down-state arrives held + automatically leased, so the
  M1 watchdog covers any failure here. }

Float Property DownSeconds = 20.0 Auto Const
{ How long you lie still before the creature moves on and you come to. }

Spell Property Wound Auto Const
{ An affliction ABILITY added as the player goes down. Ships filled with a
  vanilla lacerations affliction (ENV_AFFL_Injury_Lacerations_2) - treatable at
  any doctor. Vanilla applies afflictions via AddSpell, not Cast (they are
  ability spells). Clear it in xEdit/CK to disable the wound. }

Event OnQuestInit()
    RunConsequence()
EndEvent

Function RunConsequence()
    Actor akPlayer = Game.GetPlayer()

    If Wound != None
        akPlayer.AddSpell(Wound, true)
    EndIf
    Debug.Notification("The creature loses interest and prowls off.")

    Utility.Wait(DownSeconds)
    OSFDefeatBasicRecovery.FinishAftermath(akPlayer)

    ; Stop so OSF Defeat can Start us again on the next defeat.
    Self.Stop()
EndFunction
