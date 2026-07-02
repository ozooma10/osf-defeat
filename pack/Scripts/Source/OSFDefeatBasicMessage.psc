Scriptname OSFDefeatBasicMessage extends Quest
{ OSF Defeat - Basic Consequences: "Left as a Message".

  Fires when House Va'ruun zealots put you down (aggressor faction conditions in
  basic-message.json). Fanatics are not looters: they take nothing. They leave
  you wounded and conscious as a warning - a message. Pure flavor: no credit
  loss, just a concussion and a short down beat.

  Started natively by OSF Defeat; down-state arrives held + auto-claimed, so the
  M1 watchdog covers any failure here. }

Float Property DownSeconds = 12.0 Auto Const
{ How long you lie there before you come to. }

Spell Property Wound Auto Const
{ An affliction ABILITY added as the player goes down. Ships filled with a
  vanilla concussion affliction (ENV_AFFL_Injury_Concussion_1) - treatable at
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
    Debug.Notification("The zealots leave you breathing - as a message.")

    Utility.Wait(DownSeconds)
    OSFDefeat.RescueActor(akPlayer, True)

    ; Stop so OSF Defeat can Start us again on the next defeat.
    Self.Stop()
EndFunction
